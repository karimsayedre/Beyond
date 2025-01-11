#include "pch.h"
#include "AssimpMeshImporter.h"
#include <assimp/GltfMaterial.h>
#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Asset/AssimpAnimationImporter.h"
#include "Beyond/Asset/TextureImporter.h"
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Utilities/AssimpLogStream.h"
#include "Beyond/Renderer/Mesh.h"

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>

#include "Beyond/Core/Timer.h"
//#include "tinygltf/tiny_gltf.h"

namespace Beyond {

#define MESH_DEBUG_LOG 0

#if MESH_DEBUG_LOG
#define DEBUG_PRINT_ALL_PROPS 1
#define BEY_MESH_LOG(...) BEY_CORE_TRACE_TAG("Mesh", __VA_ARGS__)
#define BEY_MESH_ERROR(...) BEY_CORE_ERROR_TAG("Mesh", __VA_ARGS__)
#else
#define BEY_MESH_LOG(...)
#define BEY_MESH_ERROR(...)
#endif

	static const uint32_t s_MeshImportFlags =
		aiProcess_CalcTangentSpace |        // Create binormals/tangents just in case
		aiProcess_Triangulate |             // Make sure we're triangles
		aiProcess_SortByPType |             // Split meshes by primitive type
		aiProcess_ImproveCacheLocality |              // Make sure we have legit normals
		aiProcess_GenSmoothNormals |              // Make sure we have legit normals
		aiProcess_GenUVCoords |             // Convert UVs if required 
		//		aiProcess_OptimizeGraph |
		aiProcess_OptimizeMeshes |          // Batch draws where possible
		aiProcess_JoinIdenticalVertices |
		aiProcess_LimitBoneWeights |        // If more than N (=4) bone weights, discard least influencing bones and renormalise sum to 1
		aiProcess_ValidateDataStructure |   // Validation
		aiProcess_GlobalScale;              // e.g. convert cm to m for fbx import (and other formats where cm is native)

	namespace Utils {

		glm::mat4 Mat4FromAIMatrix4x4(const aiMatrix4x4& matrix)
		{
			glm::mat4 result;
			//the a,b,c,d in assimp is the row ; the 1,2,3,4 is the column
			result[0][0] = matrix.a1; result[1][0] = matrix.a2; result[2][0] = matrix.a3; result[3][0] = matrix.a4;
			result[0][1] = matrix.b1; result[1][1] = matrix.b2; result[2][1] = matrix.b3; result[3][1] = matrix.b4;
			result[0][2] = matrix.c1; result[1][2] = matrix.c2; result[2][2] = matrix.c3; result[3][2] = matrix.c4;
			result[0][3] = matrix.d1; result[1][3] = matrix.d2; result[2][3] = matrix.d3; result[3][3] = matrix.d4;
			return result;
		}

#if MESH_DEBUG_LOG
		void PrintNode(aiNode* node, size_t depth)
		{
			BEY_MESH_LOG("{0:^{1}}{2} {{", "", depth * 3, node->mName.C_Str());
			++depth;
			glm::vec3 translation;
			glm::quat rotationQuat;
			glm::vec3 scale;
			glm::mat4 transform = Mat4FromAIMatrix4x4(node->mTransformation);
			Math::DecomposeTransform(transform, translation, rotationQuat, scale);
			glm::vec3 rotation = glm::degrees(glm::eulerAngles(rotationQuat));

			BEY_MESH_LOG("{0:^{1}}translation: ({2:6.2f}, {3:6.2f}, {4:6.2f})", "", depth * 3, translation.x, translation.y, translation.z);
			BEY_MESH_LOG("{0:^{1}}rotation:    ({2:6.2f}, {3:6.2f}, {4:6.2f})", "", depth * 3, rotation.x, rotation.y, rotation.z);
			BEY_MESH_LOG("{0:^{1}}scale:       ({2:6.2f}, {3:6.2f}, {4:6.2f})", "", depth * 3, scale.x, scale.y, scale.z);
			for (uint32_t i = 0; i < node->mNumChildren; ++i)
			{
				PrintNode(node->mChildren[i], depth);
			}
			--depth;
			BEY_MESH_LOG("{0:^{1}}}}", "", depth * 3);
		}
#endif

	}

	AssimpMeshImporter::AssimpMeshImporter(const std::filesystem::path& path)
		: m_Thread(new Thread(path.string().c_str())), m_Path(path)
	{
		AssimpLogStream::Initialize();
	}
#if 0
	Ref<MaterialAsset> AssimpMeshImporter::CreateMaterialFromGLTF(const tinygltf::Model& model, const tinygltf::Material& material)
	{
		Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), material.name));
		materialAsset->SetDefaults();

		// Base Color
		if (!material.pbrMetallicRoughness.baseColorFactor.empty())
		{
			glm::vec4 baseColor(
				material.pbrMetallicRoughness.baseColorFactor[0],
				material.pbrMetallicRoughness.baseColorFactor[1],
				material.pbrMetallicRoughness.baseColorFactor[2],
				material.pbrMetallicRoughness.baseColorFactor[3]
			);
			materialAsset->SetAlbedoColor(baseColor);
		}

		// Metallic and Roughness
		float metallic = (float)material.pbrMetallicRoughness.metallicFactor;
		float roughness = (float)material.pbrMetallicRoughness.roughnessFactor;
		materialAsset->SetMetalness(metallic);
		materialAsset->SetRoughness(roughness);

		materialAsset->SetTwoSided(material.doubleSided);

		// Alpha
		//material.data.alphaCutoff = static_cast<float>(material.alphaCutoff);
		if (strcmp(material.alphaMode.c_str(), "OPAQUE") == 0) materialAsset->SetTranslucency(false);
		else if (strcmp(material.alphaMode.c_str(), "BLEND") == 0) materialAsset->SetBlending(true);
		else if (strcmp(material.alphaMode.c_str(), "MASK") == 0) materialAsset->SetTranslucency(true);

		// Textures
		if (material.pbrMetallicRoughness.baseColorTexture.index >= 0)
		{
			const auto& texture = model.textures[material.pbrMetallicRoughness.baseColorTexture.index];
			const auto& image = model.images[texture.source];
			if (texture.source == -1 || model.images.size() <= texture.source) goto skipBaseColorTex;

			TextureSpecification spec;
			spec.DebugName = eastl::string(image.uri.c_str(), image.uri.size());
			spec.Format = ImageFormat::SRGBA;
			spec.Width = image.width;
			spec.Height = image.height;
			spec.CreateBindlessDescriptor = true;
			spec.Compress = true;
			spec.UsageType = TextureUsageType::Albedo;

			AssetHandle textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(image.image.data(), image.image.size()));
			Ref<Texture2D> albedoTexture = AssetManager::GetAsset<Texture2D>(textureHandle);
			materialAsset->SetAlbedoMap(albedoTexture);
		}
	skipBaseColorTex:


		// Emissive Map
		if (material.emissiveTexture.index >= 0)
		{
			const auto& texture = model.textures[material.emissiveTexture.index];
			const auto& image = model.images[texture.source];
			if (texture.source == -1 || model.images.size() <= texture.source) goto skipEmissionTex;

			TextureSpecification spec;
			spec.DebugName = eastl::string(image.uri.c_str(), image.uri.size());
			spec.Width = image.width;
			spec.Height = image.height;
			spec.CreateBindlessDescriptor = true;
			spec.Compress = true;
			spec.UsageType = TextureUsageType::Emission;

			AssetHandle textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(image.image.data(), image.image.size()));
			materialAsset->SetEmissionMap(AssetManager::GetAsset<Texture2D>(textureHandle));
		}
	skipEmissionTex:

		// Normal Map
		if (material.normalTexture.index >= 0)
		{
			const auto& texture = model.textures[material.normalTexture.index];
			const auto& image = model.images[texture.source];
			if (texture.source == -1 || model.images.size() <= texture.source) goto skipNormalTex;

			TextureSpecification spec;
			spec.DebugName = eastl::string(image.uri.c_str(), image.uri.size());
			spec.Width = image.width;
			spec.Height = image.height;
			spec.CreateBindlessDescriptor = true;
			spec.Compress = true;
			spec.UsageType = TextureUsageType::Normal;

			AssetHandle textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(image.image.data(), image.image.size()));
			materialAsset->SetNormalMap(AssetManager::GetAsset<Texture2D>(textureHandle));
			materialAsset->SetUseNormalMap(true);
		}
	skipNormalTex:

		// metallic Roughness
		if (material.pbrMetallicRoughness.metallicRoughnessTexture.index >= 0)
		{
			const auto& texture = model.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
			const auto& image = model.images[texture.source];
			if (texture.source == -1 || model.images.size() <= texture.source) goto skipMetalnessRoughnessTex;

			TextureSpecification spec;
			spec.DebugName = eastl::string(image.uri.c_str(), image.uri.size());
			spec.Format = ImageFormat::RGBA;
			spec.Width = image.width;
			spec.Height = image.height;
			spec.CreateBindlessDescriptor = true;
			spec.Compress = true;
			spec.UsageType = TextureUsageType::Albedo;

			AssetHandle textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(image.image.data(), image.image.size()));
			Ref<Texture2D> tex = AssetManager::GetAsset<Texture2D>(textureHandle);
			materialAsset->SetMetalnessMap(tex);
			materialAsset->SetRoughnessMap(tex);
		}
	skipMetalnessRoughnessTex:

		return materialAsset;
	}
#endif

	Ref<MeshSource> AssimpMeshImporter::ImportToMeshSource()
	{
		Timer timer;
		Ref<MeshSource> meshSource = Ref<MeshSource>::Create();

		BEY_CORE_INFO_TAG("Mesh", "Loading mesh: {0}", m_Path.string());

		meshSource->SetFlag(AssetFlag::StillLoading, true);



		m_Thread->Dispatch([meshSource, path = m_Path]() mutable -> void
		{

			Assimp::Importer importer;
			//importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

			const aiScene* scene = importer.ReadFile(path.string(), s_MeshImportFlags);
			if (!scene /* || !scene->HasMeshes()*/)  // note: scene can legit contain no meshes (e.g. it could contain an armature, an animation, and no skin (mesh)))
			{
				BEY_CORE_ERROR_TAG("Mesh", "Failed to load mesh file: {0}", path.string());
				meshSource->SetFlag(AssetFlag::Invalid);
			}

			meshSource->m_Skeleton = AssimpAnimationImporter::ImportSkeleton(scene);
			BEY_CORE_INFO_TAG("Animation", "Skeleton {0} found in mesh file '{1}'", meshSource->HasSkeleton() ? "" : "not", path.string());

			meshSource->m_Animations.resize(scene->mNumAnimations);
			meshSource->m_AnimationNames.reserve(scene->mNumAnimations);
			for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
			{
				meshSource->m_AnimationNames.emplace_back(scene->mAnimations[i]->mName.C_Str());
			}

			// Actual load of the animations is deferred until later.
			// Because:
			// 1. If there is no skin (mesh), then assimp will not have loaded the skeleton, and we cannot
			//    load the animations until we know what the skeleton is
			// 2. Loading the animation requires some extra parameters to control how to import the root motion
			//    This constructor has no way of knowing what those parameters are.

			// If no meshes in the scene, there's nothing more for us to do
			if (scene->HasMeshes())
			{
				uint32_t vertexCount = 0;
				uint32_t indexCount = 0;

				meshSource->m_BoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
				meshSource->m_BoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

				meshSource->m_Submeshes.reserve(scene->mNumMeshes);
				for (unsigned m = 0; m < scene->mNumMeshes; m++)
				{
					aiMesh* mesh = scene->mMeshes[m];

					Submesh& submesh = meshSource->m_Submeshes.emplace_back();
					submesh.BaseVertex = vertexCount;
					submesh.BaseIndex = indexCount;
					submesh.MaterialIndex = mesh->mMaterialIndex;
					submesh.VertexCount = mesh->mNumVertices;
					submesh.IndexCount = mesh->mNumFaces * 3;
					submesh.MeshName = mesh->mName.C_Str();

					vertexCount += mesh->mNumVertices;
					indexCount += submesh.IndexCount;

					BEY_CORE_ASSERT(mesh->HasPositions(), "Meshes require positions.");
					BEY_CORE_ASSERT(mesh->HasNormals(), "Meshes require normals.");

					// Vertices
					auto& aabb = submesh.BoundingBox;
					aabb.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
					aabb.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
					for (size_t i = 0; i < mesh->mNumVertices; i++)
					{
						Vertex vertex;
						vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
						vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
						aabb.Min.x = glm::min(vertex.Position.x, aabb.Min.x);
						aabb.Min.y = glm::min(vertex.Position.y, aabb.Min.y);
						aabb.Min.z = glm::min(vertex.Position.z, aabb.Min.z);
						aabb.Max.x = glm::max(vertex.Position.x, aabb.Max.x);
						aabb.Max.y = glm::max(vertex.Position.y, aabb.Max.y);
						aabb.Max.z = glm::max(vertex.Position.z, aabb.Max.z);

						if (mesh->HasTangentsAndBitangents())
						{
							vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
							vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
						}

						if (mesh->HasTextureCoords(0))
							vertex.Texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

						meshSource->m_Vertices.push_back(vertex);
					}

					// Indices
					for (size_t i = 0; i < mesh->mNumFaces; i++)
					{
						BEY_CORE_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices.");
						Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
						meshSource->m_Indices.push_back(index);

						meshSource->m_TriangleCache[m].emplace_back(meshSource->m_Vertices[index.V1 + submesh.BaseVertex], meshSource->m_Vertices[index.V2 + submesh.BaseVertex], meshSource->m_Vertices[index.V3 + submesh.BaseVertex]);
					}
				}

#if MESH_DEBUG_LOG
				BEY_CORE_INFO_TAG("Mesh", "Traversing nodes for scene '{0}'", m_Path);
				Utils::PrintNode(scene->mRootNode, 0);
#endif

				MeshNode& rootNode = meshSource->m_Nodes.emplace_back();
				TraverseNodes(meshSource, scene->mRootNode, 0);

				for (const auto& submesh : meshSource->m_Submeshes)
				{
					AABB transformedSubmeshAABB = submesh.BoundingBox;
					glm::vec3 min = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Min, 1.0f));
					glm::vec3 max = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Max, 1.0f));

					meshSource->m_BoundingBox.Min.x = glm::min(meshSource->m_BoundingBox.Min.x, min.x);
					meshSource->m_BoundingBox.Min.y = glm::min(meshSource->m_BoundingBox.Min.y, min.y);
					meshSource->m_BoundingBox.Min.z = glm::min(meshSource->m_BoundingBox.Min.z, min.z);
					meshSource->m_BoundingBox.Max.x = glm::max(meshSource->m_BoundingBox.Max.x, max.x);
					meshSource->m_BoundingBox.Max.y = glm::max(meshSource->m_BoundingBox.Max.y, max.y);
					meshSource->m_BoundingBox.Max.z = glm::max(meshSource->m_BoundingBox.Max.z, max.z);
				}
			}

			// Bones
			if (meshSource->HasSkeleton())
			{
				meshSource->m_BoneInfluences.resize(meshSource->m_Vertices.size());
				for (uint32_t m = 0; m < scene->mNumMeshes; m++)
				{
					aiMesh* mesh = scene->mMeshes[m];
					Submesh& submesh = meshSource->m_Submeshes[m];

					if (mesh->mNumBones > 0)
					{
						submesh.IsRigged = true;
						for (uint32_t i = 0; i < mesh->mNumBones; i++)
						{
							aiBone* bone = mesh->mBones[i];
							bool hasNonZeroWeight = false;
							for (size_t j = 0; j < bone->mNumWeights; j++)
							{
								if (bone->mWeights[j].mWeight > 0.000001f)
								{
									hasNonZeroWeight = true;
								}
							}
							if (!hasNonZeroWeight)
								continue;

							// Find bone in skeleton
							uint32_t boneIndex = meshSource->m_Skeleton->GetBoneIndex(bone->mName.C_Str());
							if (boneIndex == Skeleton::NullIndex)
							{
								BEY_CORE_ERROR_TAG("Animation", "Could not find mesh bone '{}' in skeleton!", bone->mName.C_Str());
							}

							uint32_t boneInfoIndex = ~0;
							for (size_t j = 0; j < meshSource->m_BoneInfo.size(); ++j)
							{
								// note: Same bone could influence different submeshes (and each will have different transforms in the bind pose).
								//       Hence the need to differentiate on submesh index here.
								if ((meshSource->m_BoneInfo[j].BoneIndex == boneIndex) && (meshSource->m_BoneInfo[j].SubMeshIndex == m))
								{
									boneInfoIndex = static_cast<uint32_t>(j);
									break;
								}
							}
							if (boneInfoIndex == ~0)
							{
								boneInfoIndex = static_cast<uint32_t>(meshSource->m_BoneInfo.size());
								meshSource->m_BoneInfo.emplace_back(glm::inverse(submesh.Transform), Utils::Mat4FromAIMatrix4x4(bone->mOffsetMatrix), m, boneIndex);
							}

							for (size_t j = 0; j < bone->mNumWeights; j++)
							{
								int VertexID = submesh.BaseVertex + bone->mWeights[j].mVertexId;
								float Weight = bone->mWeights[j].mWeight;
								meshSource->m_BoneInfluences[VertexID].AddBoneData(boneInfoIndex, Weight);
							}
						}
					}
				}

				for (auto& boneInfluence : meshSource->m_BoneInfluences)
				{
					boneInfluence.NormalizeWeights();
				}
			}

			if (!meshSource->m_Vertices.empty())
				meshSource->m_VertexBuffer = VertexBuffer::Create(meshSource->m_Vertices.data(), (uint32_t)(meshSource->m_Vertices.size() * sizeof(Vertex)), path.string());

			if (meshSource->HasSkeleton())
			{
				meshSource->m_BoneInfluenceBuffer = VertexBuffer::Create(meshSource->m_BoneInfluences.data(), (uint32_t)(meshSource->m_BoneInfluences.size() * sizeof(BoneInfluence)), path.string());
			}

			if (!meshSource->m_Indices.empty())
				meshSource->m_IndexBuffer = IndexBuffer::Create(meshSource->m_Indices.data(), path.string(), (uint32_t)(meshSource->m_Indices.size() * sizeof(Index)));



			// Materials
			Ref<Texture2D> whiteTexture = Renderer::GetWhiteTexture();
/*			if (path.extension() == ".gltf" || path.extension() == ".glb")
			{
				tinygltf::Model model;
				tinygltf::TinyGLTF loader;
				std::string err, warn;
				bool ret;
				if (path.extension() == ".gltf")
					ret = loader.LoadASCIIFromFile(&model, &err, &warn, path.string());
				else if (path.extension() == ".glb")
					ret = loader.LoadBinaryFromFile(&model, &err, &warn, path.string());
				if (!ret)
				{
					BEY_CORE_ERROR_TAG("Mesh", "Failed to load GLTF file: {0}", path);
					meshSource->SetFlag(AssetFlag::Invalid);
				}

				if (!model.materials.empty())
				{
					meshSource->m_Materials.resize(model.materials.size());
					for (uint32_t i = 0; const auto & material : model.materials)
					{
						Ref<MaterialAsset> materialAsset = CreateMaterialFromGLTF(model, material);
						Beyond::Ref<Material> mi = materialAsset->GetMaterial();
						meshSource->m_Materials[i++] = mi;
					}
				}
				else
				{

					Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), "Beyond-Default"));
					materialAsset->SetDefaults();

					meshSource->m_Materials.push_back(materialAsset->GetMaterial());
				}
			}
			else */if (scene->HasMaterials())
			{
				BEY_MESH_LOG("---- Materials - {0} ----", m_Path);

				meshSource->m_Materials.resize(scene->mNumMaterials);

				for (uint32_t i = 0; i < scene->mNumMaterials; i++)
				{
					auto aiMaterial = scene->mMaterials[i];
					auto aiMaterialName = aiMaterial->GetName();
					Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), aiMaterialName.data));
					materialAsset->SetDefaults();
					Beyond::Ref<Material> mi = materialAsset->GetMaterial();
					meshSource->m_Materials[i] = mi;

					BEY_MESH_LOG("  {0} (Index = {1})", aiMaterialName.data, i);

					aiString aiTexPath;

					glm::vec4 albedoColor(0.8f);
					float emission = 0.0f;
					float ior = 1.0f;
					//int hasTransparency = 0;
					aiColor4D aiColor, aiIOR;
					if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
					{
						albedoColor = { aiColor.r, aiColor.g, aiColor.b, aiColor.a };
						if (aiColor.a < 1.0)
							mi->SetFlag(MaterialFlag::Translucent);
					}

					//// Retrieve the emissive factor
					bool emissionFallback = true;
					//if (aiMaterial->Get(AI_MATKEY_GLTF_EMISSIVE_STRENGTH, emission) != AI_SUCCESS)
					//{
					//	emissionFallback = true;
					//}

					if (emissionFallback)
					{
						aiColor3D emissiveFactor(0.f, 0.f, 0.f);
						if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveFactor) == AI_SUCCESS)
							emission = glm::dot(glm::vec3{ emissiveFactor.r, emissiveFactor.g, emissiveFactor.b }, glm::vec3{ 0.2126f, 0.7152f, 0.0722f });
					}

					if (aiMaterial->Get(AI_MATKEY_REFRACTI, ior) == AI_SUCCESS)
						ior = aiIOR.r;

					//aiColor3D specularColor;
					//if (aiMaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, specularColor) == AI_SUCCESS)
					//	materialAsset->SetSpecularColor({ specularColor.r, specularColor.g, specularColor.b });

					mi->Set("u_MaterialUniforms.AlbedoColor", albedoColor);
					//mi->Set("u_MaterialUniforms.Emission", emission);
					mi->Set("u_MaterialUniforms.IOR", ior);

					float roughness;
					float metalness;
					if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)
						roughness = 0.5f; // Default value

					if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != aiReturn_SUCCESS && aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
						metalness = 0.0f;

					float specular = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, specular) == AI_SUCCESS)
						materialAsset->SetSpecular(specular);

					float transmission = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission) == AI_SUCCESS)
						materialAsset->SetTransmission(transmission);

					float clearCoatFactor = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearCoatFactor) == AI_SUCCESS)
						materialAsset->SetClearcoat(clearCoatFactor);

					float clearCoatRoughness = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, clearCoatRoughness) == AI_SUCCESS)
						materialAsset->SetClearcoatRoughness(clearCoatRoughness);

					aiColor3D specularColor;
					if (aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS)
						materialAsset->SetSpecularColor({ specularColor.r, specularColor.g, specularColor.b });

					aiColor3D sheenColor;
					if (aiMaterial->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheenColor) == AI_SUCCESS)
						materialAsset->SetSheenColor({ sheenColor.r, sheenColor.g, sheenColor.b });

					float sheenRoughness = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_SHEEN_ROUGHNESS_FACTOR, sheenRoughness) == AI_SUCCESS)
						materialAsset->SetSheenRoughness(sheenRoughness);

					float thickness = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_VOLUME_THICKNESS_FACTOR, thickness) == AI_SUCCESS)
						materialAsset->SetThickness(thickness);

					float attenuationDistance = 0.0f;
					if (aiMaterial->Get(AI_MATKEY_VOLUME_ATTENUATION_DISTANCE, attenuationDistance) == AI_SUCCESS)
						materialAsset->SetAttenuationDistance(attenuationDistance);

					aiColor3D attenuationColor;
					if (aiMaterial->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, attenuationColor) == AI_SUCCESS)
						materialAsset->SetAttenuationColor({ attenuationColor.r, attenuationColor.g, attenuationColor.b });

					bool twoSided;
					if (aiMaterial->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
						materialAsset->SetTwoSided(twoSided);




					//if (aiMaterial->Get(AI_MATKEY_COLOR_TRANSPARENT, hasTransparency) != AI_SUCCESS)
						//hasTransparency = 0;

					// Physically realistic materials are either metal (1.0) or not (0.0)
					// Blender uses 0.5 as a default which seems wrong - materials are either metal or they are not.
					if (metalness < 0.9f)
						metalness = 0.0f;
					else
						metalness = 1.0f;

					BEY_MESH_LOG("    COLOR = {0}, {1}, {2}", aiColor.r, aiColor.g, aiColor.b);
					BEY_MESH_LOG("    ROUGHNESS = {0}", roughness);
					BEY_MESH_LOG("    METALNESS = {0}", metalness);
					bool hasAlbedoMap = aiMaterial->GetTexture(AI_MATKEY_BASE_COLOR_TEXTURE, &aiTexPath) == AI_SUCCESS;
					if (!hasAlbedoMap)
					{
						// no PBR base color. Try old-school diffuse  (note: should probably combine with specular in this case)
						hasAlbedoMap = aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &aiTexPath) == AI_SUCCESS;
					}
					bool fallback = !hasAlbedoMap;
					if (hasAlbedoMap)
					{
						AssetHandle textureHandle = 0;
						TextureSpecification spec;
						spec.DebugName = aiTexPath.C_Str();
						spec.Format = ImageFormat::SRGBA;
						spec.CreateBindlessDescriptor = true;
						spec.Compress = true;
						spec.UsageType = TextureUsageType::Albedo;
						if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
						{
							spec.Width = aiTexEmbedded->mWidth;
							spec.Height = aiTexEmbedded->mHeight;
							spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Albedo Tex from: {}", path.string());
							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
						}
						else
						{
							// TODO: Temp - this should be handled by Beyond's filesystem
							auto parentPath = path.parent_path();
							parentPath /= std::string(aiTexPath.data);
							std::string texturePath = parentPath.string();
							BEY_MESH_LOG("    Albedo map path = {0}", texturePath);
							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
						}

						Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
						if (texture && texture->IsStillLoading())
						{
							mi->Set("u_MaterialUniforms.AlbedoTexIndex", texture);
							if (texture->IsTransparent())
								mi->SetFlag(MaterialFlag::Translucent);
							mi->Set("u_MaterialUniforms.AlbedoColor", glm::vec4(1.0f));
						}
						else
						{
							BEY_CORE_ERROR_TAG("Mesh", "Could not load texture: {0}", aiTexPath.C_Str());
							fallback = true;
						}
					}

					if (fallback)
					{
						BEY_MESH_LOG("    No albedo map");
						mi->Set("u_MaterialUniforms.AlbedoTexIndex", whiteTexture);
					}

					// Normal maps
					bool hasNormalMap = aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &aiTexPath) == AI_SUCCESS;
					fallback = !hasNormalMap;
					if (hasNormalMap)
					{
						AssetHandle textureHandle = 0;

						TextureSpecification spec;
						spec.DebugName = aiTexPath.C_Str();
						spec.CreateBindlessDescriptor = true;
						spec.Compress = true;
						spec.UsageType = TextureUsageType::Normal;
						if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
						{
							//spec.Format = ImageFormat::RGB;
							spec.Width = aiTexEmbedded->mWidth;
							spec.Height = aiTexEmbedded->mHeight;
							spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Normal Tex from: {}", path.string());
							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
						}
						else
						{

							// TODO: Temp - this should be handled by Beyond's filesystem
							auto parentPath = path.parent_path();
							parentPath /= std::string(aiTexPath.data);
							std::string texturePath = parentPath.string();
							BEY_MESH_LOG("    Normal map path = {0}", texturePath);
							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
						}

						Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
						if (texture && texture->IsStillLoading())
						{
							mi->Set("u_MaterialUniforms.NormalTexIndex", texture);
							mi->Set("u_MaterialUniforms.UseNormalMap", true);
						}
						else
						{
							BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", aiTexPath.C_Str());
							fallback = true;
						}
					}

					if (fallback)
					{
						BEY_MESH_LOG("    No normal map");
						mi->Set("u_MaterialUniforms.NormalTexIndex", whiteTexture);
						mi->Set("u_MaterialUniforms.UseNormalMap", false);
					}

					// Roughness map
					bool hasRoughnessMap = aiMaterial->GetTexture(AI_MATKEY_ROUGHNESS_TEXTURE, &aiTexPath) == AI_SUCCESS;
					bool invertRoughness = false;
					if (!hasRoughnessMap)
					{
						// no PBR roughness. Try old-school shininess.  (note: this also picks up the gloss texture from PBR specular/gloss workflow).
						// Either way, Roughness = (1 - shininess)
						hasRoughnessMap = aiMaterial->GetTexture(aiTextureType_SHININESS, 0, &aiTexPath) == AI_SUCCESS;
						invertRoughness = true;
					}
					fallback = !hasRoughnessMap;
					AssetHandle roughnessTextureHandle = 0;
					if (hasRoughnessMap)
					{
						TextureSpecification spec;
						spec.DebugName = aiTexPath.C_Str();
						spec.CreateBindlessDescriptor = true;
						spec.Compress = true;
						spec.UsageType = TextureUsageType::MetalnessRoughness;
						if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
						{
							spec.Format = ImageFormat::RGBA;
							spec.Width = aiTexEmbedded->mWidth;
							spec.Height = aiTexEmbedded->mHeight;
							spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Roughness Tex from: {}", path.string());
							aiTexel* texels = aiTexEmbedded->pcData;
							if (invertRoughness)
							{
								if (spec.Height == 0)
								{
									auto buffer = TextureImporter::ToBufferFromMemory(Buffer(aiTexEmbedded->pcData, spec.Width), spec);
									texels = (aiTexel*)buffer.Data;
								}
								for (uint32_t i = 0; i < spec.Width * spec.Height; ++i)
								{
									auto& texel = texels[i];
									texel.r = 255 - texel.r;
									texel.g = 255 - texel.g;
									texel.b = 255 - texel.b;
								}
							}
							roughnessTextureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(texels, 1));
						}
						else
						{
							// TODO: Temp - this should be handled by Beyond's filesystem
							auto parentPath = path.parent_path();
							parentPath /= std::string(aiTexPath.data);
							std::filesystem::path texturePath = parentPath.string();
							BEY_MESH_LOG("    Roughness map path = {0}", texturePath);
							std::atomic_bool found = false;
							auto buffers = TextureImporter::ToBufferFromFile(texturePath, found, spec);
							for (auto& buffer : buffers)
							{
								aiTexel* texels = (aiTexel*)buffer.Data;
								if (invertRoughness)
								{
									for (uint32_t i = 0; i < spec.Width * spec.Height; i += 4)
									{
										aiTexel& texel = texels[i];
										texel.r = 255 - texel.r;
										texel.g = 255 - texel.g;
										texel.b = 255 - texel.b;
									}
								}
							}
							roughnessTextureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, buffers);
						}

						Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(roughnessTextureHandle);
						if (texture && texture->IsStillLoading())
						{
							mi->Set("u_MaterialUniforms.RoughnessTexIndex", texture);
							mi->Set("u_MaterialUniforms.Roughness", 1.0f);
						}
						else
						{
							BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", aiTexPath.C_Str());
							fallback = true;
						}
					}

					if (fallback)
					{
						BEY_MESH_LOG("    No roughness map");
						mi->Set("u_MaterialUniforms.RoughnessTexIndex", whiteTexture);
						mi->Set("u_MaterialUniforms.Roughness", roughness);

					}

					// Emission texture
					{
						bool hasEmissionTexture = aiMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &aiTexPath) == AI_SUCCESS || aiMaterial->GetTexture(aiTextureType_EMISSION_COLOR, 0, &aiTexPath) == AI_SUCCESS;
						fallback = !hasEmissionTexture;
						if (hasEmissionTexture)
						{
							AssetHandle textureHandle = 0;

							TextureSpecification spec;
							spec.CreateBindlessDescriptor = true;
							spec.Compress = true;
							spec.UsageType = TextureUsageType::Emission;

							if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
							{
								//spec.Format = ImageFormat::RGB;
								spec.Width = aiTexEmbedded->mWidth;
								spec.Height = aiTexEmbedded->mHeight;
								spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Emission Tex from: {}", path.string());
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
							}
							else
							{

								// TODO: Temp - this should be handled by Beyond's filesystem
								auto parentPath = path.parent_path();
								parentPath /= std::string(aiTexPath.data);
								std::string texturePath = parentPath.string();
								BEY_MESH_LOG("    Clearcoat map path = {0}", texturePath);
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
							}

							Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
							if (texture && texture->IsStillLoading())
							{
								mi->Set("u_MaterialUniforms.EmissionTexIndex", texture);
								materialAsset->SetEmission(emission);
							}
							else
							{
								BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", aiTexPath.C_Str());
								fallback = true;
							}
						}

						if (fallback)
						{
							BEY_MESH_LOG("    No Emission map");
							mi->Set("u_MaterialUniforms.EmissionTexIndex", whiteTexture);
							materialAsset->SetEmission(emission);
						}

					}

					// Clear coat texture
					{
						bool hasClearCoatTexture = aiMaterial->GetTexture(AI_MATKEY_CLEARCOAT_TEXTURE, &aiTexPath) == AI_SUCCESS;
						fallback = !hasClearCoatTexture;
						if (hasClearCoatTexture)
						{
							AssetHandle textureHandle = 0;

							TextureSpecification spec;
							spec.CreateBindlessDescriptor = true;
							spec.Compress = true;
							spec.UsageType = TextureUsageType::Clearcoat;

							if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
							{
								//spec.Format = ImageFormat::RGB;
								spec.Width = aiTexEmbedded->mWidth;
								spec.Height = aiTexEmbedded->mHeight;
								spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Clearcoat Tex from: {}", path.string());
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
							}
							else
							{

								// TODO: Temp - this should be handled by Beyond's filesystem
								auto parentPath = path.parent_path();
								parentPath /= std::string(aiTexPath.data);
								std::string texturePath = parentPath.string();
								BEY_MESH_LOG("    Clearcoat map path = {0}", texturePath);
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
							}

							Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
							if (texture && texture->IsStillLoading())
							{
								mi->Set("u_MaterialUniforms.ClearcoatTexIndex", texture);
							}
							else
							{
								BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", aiTexPath.C_Str());
								fallback = true;
							}
						}

						if (fallback)
						{
							BEY_MESH_LOG("    No Clearcoat map");
							mi->Set("u_MaterialUniforms.ClearcoatTexIndex", whiteTexture);
						}

					}

					// Transmission texture
					{
						bool hasTransmissionTexture = aiMaterial->GetTexture(AI_MATKEY_TRANSMISSION_TEXTURE, &aiTexPath) == AI_SUCCESS;
						fallback = !hasTransmissionTexture;
						if (hasTransmissionTexture)
						{
							AssetHandle textureHandle = 0;

							TextureSpecification spec;
							spec.CreateBindlessDescriptor = true;
							spec.Compress = true;
							spec.UsageType = TextureUsageType::Transmission;

							if (auto aiTexEmbedded = scene->GetEmbeddedTexture(aiTexPath.C_Str()))
							{
								//spec.Format = ImageFormat::RGB;
								spec.Width = aiTexEmbedded->mWidth;
								spec.Height = aiTexEmbedded->mHeight;
								spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Transmission Tex from: {}", path.string());
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
							}
							else
							{

								// TODO: Temp - this should be handled by Beyond's filesystem
								auto parentPath = path.parent_path();
								parentPath /= std::string(aiTexPath.data);
								std::string texturePath = parentPath.string();
								BEY_MESH_LOG("    Transmission map path = {0}", texturePath);
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
							}

							Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
							if (texture && texture->IsStillLoading())
							{
								materialAsset->SetTransmissionMap(texture);
								materialAsset->SetTransmission(1.0f);
							}
							else
							{
								BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", aiTexPath.C_Str());
								fallback = true;
							}
						}

						if (fallback)
						{
							BEY_MESH_LOG("    No Transmission map");
							materialAsset->SetTransmissionMap(whiteTexture);
							materialAsset->SetTransmission(transmission);
						}

					}

#if 0
					// Metalness map (or is it??)
					if (aiMaterial->Get("$raw.ReflectionFactor|file", aiPTI_String, 0, aiTexPath) == AI_SUCCESS)
					{
						// TODO: Temp - this should be handled by Beyond's filesystem
						std::filesystem::path path = filename;
						auto parentPath = path.parent_path();
						parentPath /= eastl::string(aiTexPath.data);
						eastl::string texturePath = parentPath.string();

						auto texture = Texture2D::Create(texturePath);
						if (texture->LoadedOrWaiting())
						{
							BEY_MESH_LOG("    Metalness map path = {0}", texturePath);
							mi->Set("u_MetalnessTexture", texture);
							mi->Set("u_MetalnessTexToggle", 1.0f);
						}
						else
						{
							BEY_CORE_ERROR_TAG("Mesh", "Could not load texture: {0}", texturePath);
						}
					}
					else
					{
						BEY_MESH_LOG("    No metalness texture");
						mi->Set("u_Metalness", metalness);
					}
#endif

					bool metalnessTextureFound = false;
					for (uint32_t p = 0; p < aiMaterial->mNumProperties; p++)
					{
						auto prop = aiMaterial->mProperties[p];

#if DEBUG_PRINT_ALL_PROPS
						BEY_MESH_LOG("Material Property:");
						BEY_MESH_LOG("  Name = {0}", prop->mKey.data);
						// BEY_MESH_LOG("  Type = {0}", prop->mType);
						// BEY_MESH_LOG("  Size = {0}", prop->mDataLength);
					// BEY_MESH_LOG("  Size = {0}", prop->mDataLength);
						float data = *(float*)prop->mData;
						BEY_MESH_LOG("  Value = {0}", data);

						switch (prop->mSemantic)
						{
							case aiTextureType_NONE:
								BEY_MESH_LOG("  Semantic = aiTextureType_NONE");
								break;
							case aiTextureType_DIFFUSE:
								BEY_MESH_LOG("  Semantic = aiTextureType_DIFFUSE");
								break;
							case aiTextureType_SPECULAR:
								BEY_MESH_LOG("  Semantic = aiTextureType_SPECULAR");
								break;
							case aiTextureType_AMBIENT:
								BEY_MESH_LOG("  Semantic = aiTextureType_AMBIENT");
								break;
							case aiTextureType_EMISSIVE:
								BEY_MESH_LOG("  Semantic = aiTextureType_EMISSIVE");
								break;
							case aiTextureType_HEIGHT:
								BEY_MESH_LOG("  Semantic = aiTextureType_HEIGHT");
								break;
							case aiTextureType_NORMALS:
								BEY_MESH_LOG("  Semantic = aiTextureType_NORMALS");
								break;
							case aiTextureType_SHININESS:
								BEY_MESH_LOG("  Semantic = aiTextureType_SHININESS");
								break;
							case aiTextureType_OPACITY:
								BEY_MESH_LOG("  Semantic = aiTextureType_OPACITY");
								break;
							case aiTextureType_DISPLACEMENT:
								BEY_MESH_LOG("  Semantic = aiTextureType_DISPLACEMENT");
								break;
							case aiTextureType_LIGHTMAP:
								BEY_MESH_LOG("  Semantic = aiTextureType_LIGHTMAP");
								break;
							case aiTextureType_REFLECTION:
								BEY_MESH_LOG("  Semantic = aiTextureType_REFLECTION");
								break;
							case aiTextureType_UNKNOWN:
								BEY_MESH_LOG("  Semantic = aiTextureType_UNKNOWN");
								break;
						}
#endif

						if (prop->mType == aiPTI_String)
						{
							uint32_t strLength = *(uint32_t*)prop->mData;
							std::string str(prop->mData + 4, strLength);

							eastl::string key = prop->mKey.data;
							if (key == "$raw.ReflectionFactor|file")
							{
								AssetHandle textureHandle = 0;
								TextureSpecification spec;
								spec.DebugName = str.c_str();
								spec.CreateBindlessDescriptor = true;
								spec.Compress = true;
								spec.UsageType = TextureUsageType::MetalnessRoughness;
								if (auto aiTexEmbedded = scene->GetEmbeddedTexture(str.data()))
								{
									//spec.Format = ImageFormat::RGB;
									spec.Width = aiTexEmbedded->mWidth;
									spec.Height = aiTexEmbedded->mHeight;
									spec.DebugName = aiTexEmbedded->mFilename.C_Str();
									textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
								}
								else
								{
									// TODO: Temp - this should be handled by Beyond's filesystem
									auto parentPath = path.parent_path();
									parentPath /= str;
									std::string texturePath = parentPath.string();
									BEY_MESH_LOG("    Metalness map path = {0}", texturePath);
									textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
								}

								Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
								if (texture && texture->IsStillLoading())
								{
									metalnessTextureFound = true;
									mi->Set("u_MaterialUniforms.MetalnessTexIndex", texture);
									mi->Set("u_MaterialUniforms.Metalness", 1.0f);
								}
								else
								{
									BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", str);
								}
								break;
							}
						}
					}


					if (!metalnessTextureFound)
					{

						aiString metalnessTexturePath;
						metalnessTextureFound = aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &metalnessTexturePath) == AI_SUCCESS;
						fallback = !metalnessTextureFound;
						if (metalnessTextureFound)
						{

							AssetHandle textureHandle = 0;
							TextureSpecification spec;
							spec.DebugName = metalnessTexturePath.C_Str();
							spec.CreateBindlessDescriptor = true;
							spec.Compress = true;
							spec.UsageType = TextureUsageType::MetalnessRoughness;
							if (auto aiTexEmbedded = scene->GetEmbeddedTexture(metalnessTexturePath.C_Str()))
							{
								//spec.Format = ImageFormat::RGB;
								spec.Width = aiTexEmbedded->mWidth;
								spec.Height = aiTexEmbedded->mHeight;
								spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Metalness Tex from: {}", path.string());
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
							}
							else
							{
								// TODO: Temp - this should be handled by Beyond's filesystem
								auto parentPath = path.parent_path();
								parentPath /= metalnessTexturePath.C_Str();
								std::string texturePath = parentPath.string();
								BEY_MESH_LOG("    Metalness map path = {0}", texturePath);
								textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
							}

							Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
							if (texture && texture->IsStillLoading())
							{
								metalnessTextureFound = true;
								mi->Set("u_MaterialUniforms.MetalnessTexIndex", texture);
								mi->Set("u_MaterialUniforms.Metalness", 1.0f);
							}
							else
							{
								BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", metalnessTexturePath.C_Str());
							}
						}
					}


					fallback = !metalnessTextureFound;
					if (fallback)
					{
						BEY_MESH_LOG("    No metalness map");
						mi->Set("u_MaterialUniforms.MetalnessTexIndex", whiteTexture);
						mi->Set("u_MaterialUniforms.Metalness", metalness);

					}
				}
				BEY_MESH_LOG("------------------------");
			}
			else
			{
				if (scene->HasMeshes())
				{
					Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), "Beyond-Default"));
					materialAsset->SetDefaults();

					meshSource->m_Materials.push_back(materialAsset->GetMaterial());
				}
			}
			Renderer::Submit([meshSource]() mutable
			{
				meshSource->m_IsReady = true;
				meshSource->SetFlag(AssetFlag::StillLoading, false);
			});

		});



		m_Thread->Join();
		BEY_CORE_INFO("It took {}s to load {}", timer.Elapsed(), m_Path);

		return meshSource;
	}

	bool AssimpMeshImporter::ImportSkeleton(Scope<Skeleton>& skeleton)
	{
		Assimp::Importer importer;
		//importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

		const aiScene* scene = importer.ReadFile(m_Path.string(), s_MeshImportFlags);
		if (!scene)
		{
			BEY_CORE_ERROR_TAG("Animation", "Failed to load mesh source file: {0}", m_Path.string());
			return false;
		}

		skeleton = AssimpAnimationImporter::ImportSkeleton(scene);
		return true;
	}

	bool AssimpMeshImporter::ImportAnimation(const uint32_t animationIndex, const Skeleton& skeleton, const bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask, Scope<Animation>& animation)
	{
		Assimp::Importer importer;
		//importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

		const aiScene* scene = importer.ReadFile(m_Path.string(), s_MeshImportFlags);
		if (!scene)
		{
			BEY_CORE_ERROR_TAG("Animation", "Failed to load mesh source file: {0}", m_Path.string());
			return false;
		}

		if (animationIndex >= scene->mNumAnimations)
		{
			BEY_CORE_ERROR_TAG("Animation", "Animation index {0} out of range for mesh source file: {1}", animationIndex, m_Path.string());
			return false;
		}

		animation = AssimpAnimationImporter::ImportAnimation(scene, animationIndex, skeleton, isMaskedRootMotion, rootTranslationMask, rootRotationMask);
		return true;
	}

	bool AssimpMeshImporter::IsCompatibleSkeleton(const uint32_t animationIndex, const Skeleton& skeleton)
	{
		Assimp::Importer importer;
		//importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

		const aiScene* scene = importer.ReadFile(m_Path.string(), s_MeshImportFlags);
		if (!scene)
		{
			BEY_CORE_ERROR_TAG("Mesh", "Failed to load mesh file: {0}", m_Path.string());
			return false;
		}

		if (scene->mNumAnimations <= animationIndex)
		{
			return false;
		}

		const auto animationNames = AssimpAnimationImporter::GetAnimationNames(scene);
		if (animationNames.empty())
		{
			return false;
		}

		const aiAnimation* anim = scene->mAnimations[animationIndex];

		eastl::unordered_map<eastl::string_view, uint32_t> boneIndices;
		for (uint32_t i = 0; i < skeleton.GetNumBones(); ++i)
		{
			boneIndices.emplace(skeleton.GetBoneNameEA(i), i);
		}

		std::set<std::tuple<uint32_t, aiNodeAnim*>> validChannels;
		for (uint32_t channelIndex = 0; channelIndex < anim->mNumChannels; ++channelIndex)
		{
			aiNodeAnim* nodeAnim = anim->mChannels[channelIndex];
			auto it = boneIndices.find(nodeAnim->mNodeName.C_Str());
			if (it != boneIndices.end())
			{
				validChannels.emplace(it->second, nodeAnim);
			}
		}

		// It's hard to decide whether or not the animation is "valid" for the given skeleton.
		// For example an animation does not necessarily contain channels for all bones.
		// Conversely, some channels in the animation might not be for bones.
		// So, you cannot simply check number of valid channels == number of bones
		// And you cannot simply check number of invalid channels == 0
		// For now, I've just decided on a simple number of valid channels must be at least 1
		return validChannels.size() > 0;
	}

	uint32_t AssimpMeshImporter::GetAnimationCount()
	{
		Assimp::Importer importer;
		//importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);

		const aiScene* scene = importer.ReadFile(m_Path.string(), s_MeshImportFlags);
		if (!scene)
		{
			BEY_CORE_ERROR_TAG("Mesh", "Failed to load mesh file: {0}", m_Path.string());
			return false;
		}

		return (uint32_t)scene->mNumAnimations;
	}

	void AssimpMeshImporter::TraverseNodes(Ref<MeshSource> meshSource, void* assimpNode, uint32_t nodeIndex, const glm::mat4& parentTransform, uint32_t level)
	{
		aiNode* aNode = (aiNode*)assimpNode;

		MeshNode& node = meshSource->m_Nodes[nodeIndex];
		node.Name = aNode->mName.C_Str();
		node.LocalTransform = Utils::Mat4FromAIMatrix4x4(aNode->mTransformation);

		glm::mat4 transform = parentTransform * node.LocalTransform;
		for (uint32_t i = 0; i < aNode->mNumMeshes; i++)
		{
			uint32_t submeshIndex = aNode->mMeshes[i];
			auto& submesh = meshSource->m_Submeshes[submeshIndex];
			submesh.NodeName = aNode->mName.C_Str();
			submesh.Transform = transform;
			submesh.LocalTransform = node.LocalTransform;

			node.Submeshes.push_back(submeshIndex);
		}

		// BEY_MESH_LOG("{0} {1}", LevelToSpaces(level), node->mName.C_Str());

		uint32_t parentNodeIndex = (uint32_t)meshSource->m_Nodes.size() - 1;
		node.Children.resize(aNode->mNumChildren);
		for (uint32_t i = 0; i < aNode->mNumChildren; i++)
		{
			MeshNode& child = meshSource->m_Nodes.emplace_back();
			uint32_t childIndex = static_cast<uint32_t>(meshSource->m_Nodes.size()) - 1;
			child.Parent = parentNodeIndex;
			meshSource->m_Nodes[nodeIndex].Children[i] = childIndex;
			TraverseNodes(meshSource, aNode->mChildren[i], childIndex, transform, level + 1);
		}
	}


}
