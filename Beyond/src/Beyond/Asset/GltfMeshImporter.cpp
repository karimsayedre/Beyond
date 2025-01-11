#include "pch.h"
//#include <tinygltf/tiny_gltf.h>
#include "GltfMeshImporter.h"

#include "AssetManager.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Renderer/Renderer.h"

namespace Beyond {
#if 0

	namespace Utils {

		glm::mat4 Mat4FromTinyGLTF(const   std::vector<double>& aMatrix)
		{
			glm::mat4 matrix(1.0f);
			if (!aMatrix.empty())
			{
				for (int i = 0; i < 4; ++i)
				{
					for (int j = 0; j < 4; ++j)
					{
						matrix[i][j] = (float)aMatrix[i * 4 + j];
					}
				}
			}
			return matrix;
		}

	}


	GltfMeshImporter::GltfMeshImporter(const std::filesystem::path& path)
		: m_Path(path)
	{

	}
	Ref<MaterialAsset> GltfMeshImporter::CreateMaterialFromGLTF(const tinygltf::Model& model, const tinygltf::Material& material)
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

	Ref<MeshSource> GltfMeshImporter::ImportToMeshSource()
	{
		Timer timer;
		Ref<MeshSource> meshSource = Ref<MeshSource>::Create();

		BEY_CORE_INFO_TAG("Mesh", "Loading mesh: {0}", m_Path.string());

		meshSource->SetFlag(AssetFlag::StillLoading, false);

		tinygltf::Model model;
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		bool ret;
		if (m_Path.extension() == ".gltf")
			ret = loader.LoadASCIIFromFile(&model, &err, &warn, m_Path.string());
		else if (m_Path.extension() == ".glb")
			ret = loader.LoadBinaryFromFile(&model, &err, &warn, m_Path.string());
		if (!ret)
		{
			BEY_CORE_ERROR_TAG("Mesh", "Failed to load GLTF file: {0}", m_Path);
			meshSource->SetFlag(AssetFlag::Invalid);
			return nullptr;
		}

		if (!warn.empty())
		{
			BEY_CORE_WARN_TAG("Mesh", "GLTF Load Warning: {0}", warn);
		}

		if (!err.empty())
		{
			BEY_CORE_ERROR_TAG("Mesh", "GLTF Load Error: {0}", err);
		}

		// Skeleton and Animations
		// TinyGLTF handles these differently from Assimp
		if (model.skins.size() > 0)
		{
			// Import skeleton logic would be different
			//meshSource->m_Skeleton = ImportSkeletonFromGLTF(model);
		}

		// Animation names
		for (const auto& animation : model.animations)
		{
			meshSource->m_AnimationNames.emplace_back(animation.name.c_str(), animation.name.size());
		}

		// Mesh Processing
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		meshSource->m_BoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		meshSource->m_BoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

		//BEY_CORE_VERIFY(model.meshes.size() == 1, "Mesh Source is only one mesh");

		for (uint32_t meshIndex = 0; meshIndex < (uint32_t)model.meshes.size(); ++meshIndex)
		{
			const tinygltf::Mesh& gltfMesh = model.meshes[meshIndex];
			meshSource->m_Submeshes.reserve(gltfMesh.primitives.size());

			for (size_t primitiveIndex = 0; primitiveIndex < gltfMesh.primitives.size(); ++primitiveIndex)
			{
				Submesh& submesh = meshSource->m_Submeshes.emplace_back();

				tinygltf::Primitive primitive = gltfMesh.primitives[primitiveIndex];

				// Get data indices
				int indicesIndex = primitive.indices;
				int positionIndex = -1;
				int normalIndex = -1;
				int tangentIndex = -1;
				int bitangentIndex = -1;
				int uv0Index = -1;


				// Set the mesh primitive's material to the default material if one is not assigned or if no materials exist in the GLTF
				if (primitive.material == -1) primitive.material = 0;

				// Get a reference to the mesh primitive's material
				// If the mesh primitive material is blended or masked, it is not opaque
				//const Material& mat = model.materials[primitive.material];
				//if (mat.data.alphaMode != 0) primitive.opaque = false;


				if (primitive.attributes.count("POSITION") > 0)
				{
					positionIndex = primitive.attributes.at("POSITION");
				}

				if (primitive.attributes.count("NORMAL") > 0)
				{
					normalIndex = primitive.attributes.at("NORMAL");
				}

				if (primitive.attributes.count("BINORMAL"))
				{
					bitangentIndex = primitive.attributes.at("BINORMAL");
				}

				if (primitive.attributes.count("POSITION") > 0)
				{
					positionIndex = primitive.attributes.at("POSITION");
				}

				if (primitive.attributes.count("TEXCOORD_0") > 0)
				{
					uv0Index = primitive.attributes.at("TEXCOORD_0");
				}

				// Vertex positions
				const tinygltf::Accessor& positionAccessor = model.accessors[positionIndex];
				const tinygltf::BufferView& positionBufferView = model.bufferViews[positionAccessor.bufferView];
				const tinygltf::Buffer& positionBuffer = model.buffers[positionBufferView.buffer];
				const uint8_t* positionBufferAddress = positionBuffer.data.data();
				int positionStride = tinygltf::GetComponentSizeInBytes(positionAccessor.componentType) * tinygltf::GetNumComponentsInType(positionAccessor.type);
				assert(positionStride == 12);

				// Vertex indices
				const tinygltf::Accessor& indexAccessor = model.accessors[indicesIndex];
				const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
				const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];
				const uint8_t* indexBufferAddress = indexBuffer.data.data();
				int indexStride = tinygltf::GetComponentSizeInBytes(indexAccessor.componentType) * tinygltf::GetNumComponentsInType(indexAccessor.type);
				meshSource->m_Indices.reserve(indexAccessor.count / 3);

				// Vertex normals
				tinygltf::Accessor normalAccessor;
				tinygltf::BufferView normalBufferView;
				const uint8_t* normalBufferAddress = nullptr;
				int normalStride = -1;
				if (normalIndex > -1)
				{
					normalAccessor = model.accessors[normalIndex];
					normalBufferView = model.bufferViews[normalAccessor.bufferView];
					normalStride = tinygltf::GetComponentSizeInBytes(normalAccessor.componentType) * tinygltf::GetNumComponentsInType(normalAccessor.type);
					assert(normalStride == 12);

					const tinygltf::Buffer& normalBuffer = model.buffers[normalBufferView.buffer];
					normalBufferAddress = normalBuffer.data.data();
				}

				// Vertex tangents
				tinygltf::Accessor tangentAccessor;
				tinygltf::BufferView tangentBufferView;
				const uint8_t* tangentBufferAddress = nullptr;
				int tangentStride = -1;
				if (tangentIndex > -1)
				{
					tangentAccessor = model.accessors[tangentIndex];
					tangentBufferView = model.bufferViews[tangentAccessor.bufferView];
					tangentStride = tinygltf::GetComponentSizeInBytes(tangentAccessor.componentType) * tinygltf::GetNumComponentsInType(tangentAccessor.type);
					assert(tangentStride == 16);

					const tinygltf::Buffer& tangentBuffer = model.buffers[tangentBufferView.buffer];
					tangentBufferAddress = tangentBuffer.data.data();
				}

				// Vertex bitangents
				tinygltf::Accessor bitangentAccessor;
				tinygltf::BufferView bitangentBufferView;
				const uint8_t* bitangentBufferAddress = nullptr;
				int bitangentStride = -1;
				if (bitangentIndex > -1)
				{
					bitangentAccessor = model.accessors[bitangentIndex];
					bitangentBufferView = model.bufferViews[bitangentAccessor.bufferView];
					bitangentStride = tinygltf::GetComponentSizeInBytes(bitangentAccessor.componentType) * tinygltf::GetNumComponentsInType(bitangentAccessor.type);
					assert(bitangentStride == 16);

					const tinygltf::Buffer& bitangentBuffer = model.buffers[bitangentBufferView.buffer];
					bitangentBufferAddress = bitangentBuffer.data.data();
				}

				// Vertex texture coordinates
				tinygltf::Accessor uv0Accessor;
				tinygltf::BufferView uv0BufferView;
				const uint8_t* uv0BufferAddress = nullptr;
				int uv0Stride = -1;
				if (uv0Index > -1)
				{
					uv0Accessor = model.accessors[uv0Index];
					uv0BufferView = model.bufferViews[uv0Accessor.bufferView];
					uv0Stride = tinygltf::GetComponentSizeInBytes(uv0Accessor.componentType) * tinygltf::GetNumComponentsInType(uv0Accessor.type);
					assert(uv0Stride == 8);

					const tinygltf::Buffer& uv0Buffer = model.buffers[uv0BufferView.buffer];
					uv0BufferAddress = uv0Buffer.data.data();
				}



				// Get the vertex data
				for (uint32_t vertexIndex = 0; vertexIndex < static_cast<uint32_t>(positionAccessor.count); vertexIndex++)
				{
					Vertex v;

					const uint8_t* address = positionBufferAddress + positionBufferView.byteOffset + positionAccessor.byteOffset + (vertexIndex * positionStride);
					memcpy(&v.Position, address, positionStride);

					if (normalIndex > -1)
					{
						address = normalBufferAddress + normalBufferView.byteOffset + normalAccessor.byteOffset + (vertexIndex * normalStride);
						memcpy(&v.Normal, address, normalStride);
					}

					if (tangentIndex > -1)
					{
						address = tangentBufferAddress + tangentBufferView.byteOffset + tangentAccessor.byteOffset + (vertexIndex * tangentStride);
						memcpy(&v.Tangent, address, tangentStride);
					}

					if (bitangentIndex > -1)
					{
						address = bitangentBufferAddress + bitangentBufferView.byteOffset + bitangentAccessor.byteOffset + (vertexIndex * bitangentStride);
						memcpy(&v.Binormal, address, bitangentStride);

					}

					if (uv0Index > -1)
					{
						address = uv0BufferAddress + uv0BufferView.byteOffset + uv0Accessor.byteOffset + (vertexIndex * uv0Stride);
						memcpy(&v.Texcoord, address, uv0Stride);
					}

					meshSource->m_Vertices.push_back(v);
				}


				auto& aabb = submesh.BoundingBox;
				aabb.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
				aabb.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
				for (size_t i = 0; i < meshSource->m_Vertices.size(); i++)
				{
					aabb.Min.x = glm::min(meshSource->m_Vertices[i].Position.x, aabb.Min.x);
					aabb.Min.y = glm::min(meshSource->m_Vertices[i].Position.y, aabb.Min.y);
					aabb.Min.z = glm::min(meshSource->m_Vertices[i].Position.z, aabb.Min.z);
					aabb.Max.x = glm::max(meshSource->m_Vertices[i].Position.x, aabb.Max.x);
					aabb.Max.y = glm::max(meshSource->m_Vertices[i].Position.y, aabb.Max.y);
					aabb.Max.z = glm::max(meshSource->m_Vertices[i].Position.z, aabb.Max.z);
				}

				// Get the index data
				// Indices can be either unsigned char, unsigned short, or unsigned long
				// Converting to full precision for easy use on GPU
				const uint8_t* baseAddress = indexBufferAddress + indexBufferView.byteOffset + indexAccessor.byteOffset;
				if (indexStride == 1)
				{
					std::vector<uint8_t> quarter;
					quarter.resize(indexAccessor.count);

					memcpy(quarter.data(), baseAddress, (indexAccessor.count * indexStride));

					// Convert quarter precision indices to full precision
					for (size_t i = 0; i < indexAccessor.count / 3; i++)
					{
						meshSource->m_Indices.emplace_back(quarter[3 * i + 0], quarter[3 * i + 1], quarter[3 * i + 2]);
					}
				}
				else if (indexStride == 2)
				{
					std::vector<uint16_t> half;
					half.resize(indexAccessor.count);

					memcpy(half.data(), baseAddress, (indexAccessor.count * indexStride));

					// Convert half precision indices to full precision
					for (size_t i = 0; i < indexAccessor.count / 3; i++)
					{
						meshSource->m_Indices.emplace_back(half[3 * i + 0], half[3 * i + 1], half[3 * i + 2]);

					}
				}
				else
				{
					memcpy(std::bit_cast<uint32_t*>(meshSource->m_Indices.data()) + indexCount, baseAddress, (indexAccessor.count * indexStride));
				}

				if (bitangentIndex == -1)
				{
					for (size_t i = 0; i < indexAccessor.count; i += 3)
					{
						uint32_t i0 = meshSource->m_Indices[i / 3].V1;
						uint32_t i1 = meshSource->m_Indices[i / 3].V2;
						uint32_t i2 = meshSource->m_Indices[i / 3].V3;

						glm::vec3 p0 = meshSource->m_Vertices[i0 / 3 + vertexCount].Position;
						glm::vec3 p1 = meshSource->m_Vertices[i1 / 3 + vertexCount].Position;
						glm::vec3 p2 = meshSource->m_Vertices[i2 / 3 + vertexCount].Position;

						glm::vec2 uv0 = meshSource->m_Vertices[i0 / 3 + vertexCount].Texcoord;
						glm::vec2 uv1 = meshSource->m_Vertices[i1 / 3 + vertexCount].Texcoord;
						glm::vec2 uv2 = meshSource->m_Vertices[i2 / 3 + vertexCount].Texcoord;

						glm::vec3 edge1 = p1 - p0;
						glm::vec3 edge2 = p2 - p0;
						glm::vec2 deltaUV1 = uv1 - uv0;
						glm::vec2 deltaUV2 = uv2 - uv0;

						float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
						glm::vec3 tangent = f * (deltaUV2.y * edge1 - deltaUV1.y * edge2);
						glm::vec3 bitangent = f * (deltaUV1.x * edge2 - deltaUV2.x * edge1);

						meshSource->m_Vertices[i0].Tangent += tangent;
						meshSource->m_Vertices[i1].Tangent += tangent;
						meshSource->m_Vertices[i2].Tangent += tangent;

						meshSource->m_Vertices[i0].Binormal += bitangent;
						meshSource->m_Vertices[i1].Binormal += bitangent;
						meshSource->m_Vertices[i2].Binormal += bitangent;
					}
				}

				for (const auto& index : meshSource->m_Indices)
					meshSource->m_TriangleCache[meshIndex].emplace_back(meshSource->m_Vertices[index.V1 + submesh.BaseVertex], meshSource->m_Vertices[index.V2 + submesh.BaseVertex], meshSource->m_Vertices[index.V3 + submesh.BaseVertex]);




				submesh.BaseVertex = vertexCount;
				submesh.BaseIndex = indexCount;
				submesh.MaterialIndex = primitive.material;
				submesh.IndexCount = (uint32_t)indexAccessor.count;
				submesh.VertexCount = (uint32_t)positionAccessor.count;
				submesh.MeshName = eastl::string(gltfMesh.name.c_str(), gltfMesh.name.size());

				vertexCount += (uint32_t)positionAccessor.count;
				indexCount += (uint32_t)indexAccessor.count;
			}
		}


		//// Bones
		//if (meshSource->HasSkeleton())
		//{
		//	meshSource->m_BoneInfluences.resize(meshSource->m_Vertices.size());
		//	for (uint32_t m = 0; m < scene->mNumMeshes; m++)
		//	{
		//		aiMesh* mesh = scene->mMeshes[m];
		//		Submesh& submesh = meshSource->m_Submeshes[m];

		//		if (mesh->mNumBones > 0)
		//		{
		//			submesh.IsRigged = true;
		//			for (uint32_t i = 0; i < mesh->mNumBones; i++)
		//			{
		//				aiBone* bone = mesh->mBones[i];
		//				bool hasNonZeroWeight = false;
		//				for (size_t j = 0; j < bone->mNumWeights; j++)
		//				{
		//					if (bone->mWeights[j].mWeight > 0.000001f)
		//					{
		//						hasNonZeroWeight = true;
		//					}
		//				}
		//				if (!hasNonZeroWeight)
		//					continue;

		//				// Find bone in skeleton
		//				uint32_t boneIndex = meshSource->m_Skeleton->GetBoneIndex(bone->mName.C_Str());
		//				if (boneIndex == Skeleton::NullIndex)
		//				{
		//					BEY_CORE_ERROR_TAG("Animation", "Could not find mesh bone '{}' in skeleton!", bone->mName.C_Str());
		//				}

		//				uint32_t boneInfoIndex = ~0;
		//				for (size_t j = 0; j < meshSource->m_BoneInfo.size(); ++j)
		//				{
		//					// note: Same bone could influence different submeshes (and each will have different transforms in the bind pose).
		//					//       Hence the need to differentiate on submesh index here.
		//					if ((meshSource->m_BoneInfo[j].BoneIndex == boneIndex) && (meshSource->m_BoneInfo[j].SubMeshIndex == m))
		//					{
		//						boneInfoIndex = static_cast<uint32_t>(j);
		//						break;
		//					}
		//				}
		//				if (boneInfoIndex == ~0)
		//				{
		//					boneInfoIndex = static_cast<uint32_t>(meshSource->m_BoneInfo.size());
		//					meshSource->m_BoneInfo.emplace_back(glm::inverse(submesh.Transform), Utils::Mat4FromAIMatrix4x4(bone->mOffsetMatrix), m, boneIndex);
		//				}

		//				for (size_t j = 0; j < bone->mNumWeights; j++)
		//				{
		//					int VertexID = submesh.BaseVertex + bone->mWeights[j].mVertexId;
		//					float Weight = bone->mWeights[j].mWeight;
		//					meshSource->m_BoneInfluences[VertexID].AddBoneData(boneInfoIndex, Weight);
		//				}
		//			}
		//		}
		//	}

		//	for (auto& boneInfluence : meshSource->m_BoneInfluences)
		//	{
		//		boneInfluence.NormalizeWeights();
		//	}
		//}

		if (meshSource->m_Vertices.size())
			meshSource->m_VertexBuffer = VertexBuffer::Create(meshSource->m_Vertices.data(), (uint32_t)(meshSource->m_Vertices.size() * sizeof(Vertex)), m_Path.string());

		if (meshSource->HasSkeleton())
		{
			meshSource->m_BoneInfluenceBuffer = VertexBuffer::Create(meshSource->m_BoneInfluences.data(), (uint32_t)(meshSource->m_BoneInfluences.size() * sizeof(BoneInfluence)), m_Path.string());
		}

		if (meshSource->m_Indices.size())
			meshSource->m_IndexBuffer = IndexBuffer::Create(meshSource->m_Indices.data(), m_Path.string(), (uint32_t)(meshSource->m_Indices.size() * sizeof(Index)));

		//scene.numMeshPrimitives = geometryIndex;

		// Material Processing
		if (!model.materials.empty())
		{
			meshSource->m_Materials.reserve(model.materials.size());
			for (const auto& material : model.materials)
			{
				Ref<MaterialAsset> materialAsset = CreateMaterialFromGLTF(model, material);
				meshSource->m_Materials.push_back(materialAsset->GetMaterial());
			}
		}
		else
		{

			Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), "Beyond-Default"));
			materialAsset->SetDefaults();

			meshSource->m_Materials.push_back(materialAsset->GetMaterial());
		}

		MeshNode& rootNode = meshSource->m_Nodes.emplace_back();
		TraverseNodes(meshSource, model, model.nodes.data(), 0);

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



		//
		//
		//		const aiScene* scene = importer.ReadFile(path.string(), s_MeshImportFlags);
		//		if (!scene /* || !scene->HasMeshes()*/)  // note: scene can legit contain no meshes (e.g. it could contain an armature, an animation, and no skin (mesh)))
		//		{
		//			BEY_CORE_ERROR_TAG("Mesh", "Failed to load mesh file: {0}", path.string());
		//			meshSource->SetFlag(AssetFlag::Invalid);
		//		}
		//
		//		meshSource->m_Skeleton = AssimpAnimationImporter::ImportSkeleton(scene);
		//		BEY_CORE_INFO_TAG("Animation", "Skeleton {0} found in mesh file '{1}'", meshSource->HasSkeleton() ? "" : "not", path.string());
		//
		//		meshSource->m_Animations.resize(scene->mNumAnimations);
		//		meshSource->m_AnimationNames.reserve(scene->mNumAnimations);
		//		for (uint32_t i = 0; i < scene->mNumAnimations; ++i)
		//		{
		//			meshSource->m_AnimationNames.emplace_back(scene->mAnimations[i]->mName.C_Str());
		//		}
		//
		//		// Actual load of the animations is deferred until later.
		//		// Because:
		//		// 1. If there is no skin (mesh), then assimp will not have loaded the skeleton, and we cannot
		//		//    load the animations until we know what the skeleton is
		//		// 2. Loading the animation requires some extra parameters to control how to import the root motion
		//		//    This constructor has no way of knowing what those parameters are.
		//
		//		// If no meshes in the scene, there's nothing more for us to do
		//		if (scene->HasMeshes())
		//		{
		//			uint32_t vertexCount = 0;
		//			uint32_t indexCount = 0;
		//
		//			meshSource->m_BoundingBox.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		//			meshSource->m_BoundingBox.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		//
		//			meshSource->m_Submeshes.reserve(scene->mNumMeshes);
		//			for (unsigned m = 0; m < scene->mNumMeshes; m++)
		//			{
		//				aiMesh* mesh = scene->mMeshes[m];
		//
		//				Submesh& submesh = meshSource->m_Submeshes.emplace_back();
		//				submesh.BaseVertex = vertexCount;
		//				submesh.BaseIndex = indexCount;
		//				submesh.MaterialIndex = mesh->mMaterialIndex;
		//				submesh.VertexCount = mesh->mNumVertices;
		//				submesh.IndexCount = mesh->mNumFaces * 3;
		//				submesh.MeshName = mesh->mName.C_Str();
		//
		//				vertexCount += mesh->mNumVertices;
		//				indexCount += submesh.IndexCount;
		//
		//				BEY_CORE_ASSERT(mesh->HasPositions(), "Meshes require positions.");
		//				BEY_CORE_ASSERT(mesh->HasNormals(), "Meshes require normals.");
		//
		//				// Vertices
		//				auto& aabb = submesh.BoundingBox;
		//				aabb.Min = { FLT_MAX, FLT_MAX, FLT_MAX };
		//				aabb.Max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
		//				for (size_t i = 0; i < mesh->mNumVertices; i++)
		//				{
		//					Vertex vertex;
		//					vertex.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		//					vertex.Normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };
		//					aabb.Min.x = glm::min(vertex.Position.x, aabb.Min.x);
		//					aabb.Min.y = glm::min(vertex.Position.y, aabb.Min.y);
		//					aabb.Min.z = glm::min(vertex.Position.z, aabb.Min.z);
		//					aabb.Max.x = glm::max(vertex.Position.x, aabb.Max.x);
		//					aabb.Max.y = glm::max(vertex.Position.y, aabb.Max.y);
		//					aabb.Max.z = glm::max(vertex.Position.z, aabb.Max.z);
		//
		//					if (mesh->HasTangentsAndBitangents())
		//					{
		//						vertex.Tangent = { mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z };
		//						vertex.Binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		//					}
		//
		//					if (mesh->HasTextureCoords(0))
		//						vertex.Texcoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };
		//
		//					meshSource->m_Vertices.push_back(vertex);
		//				}
		//
		//				// Indices
		//				for (size_t i = 0; i < mesh->mNumFaces; i++)
		//				{
		//					BEY_CORE_ASSERT(mesh->mFaces[i].mNumIndices == 3, "Must have 3 indices.");
		//					Index index = { mesh->mFaces[i].mIndices[0], mesh->mFaces[i].mIndices[1], mesh->mFaces[i].mIndices[2] };
		//					meshSource->m_Indices.push_back(index);
		//
		//					meshSource->m_TriangleCache[m].emplace_back(meshSource->m_Vertices[index.V1 + submesh.BaseVertex], meshSource->m_Vertices[index.V2 + submesh.BaseVertex], meshSource->m_Vertices[index.V3 + submesh.BaseVertex]);
		//				}
		//			}
		//
		//#if MESH_DEBUG_LOG
		//			BEY_CORE_INFO_TAG("Mesh", "Traversing nodes for scene '{0}'", m_Path);
		//			Utils::PrintNode(scene->mRootNode, 0);
		//#endif
		//
		//			MeshNode& rootNode = meshSource->m_Nodes.emplace_back();
		//			TraverseNodes(meshSource, scene->mRootNode, 0);
		//
		//			for (const auto& submesh : meshSource->m_Submeshes)
		//			{
		//				AABB transformedSubmeshAABB = submesh.BoundingBox;
		//				glm::vec3 min = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Min, 1.0f));
		//				glm::vec3 max = glm::vec3(submesh.Transform * glm::vec4(transformedSubmeshAABB.Max, 1.0f));
		//
		//				meshSource->m_BoundingBox.Min.x = glm::min(meshSource->m_BoundingBox.Min.x, min.x);
		//				meshSource->m_BoundingBox.Min.y = glm::min(meshSource->m_BoundingBox.Min.y, min.y);
		//				meshSource->m_BoundingBox.Min.z = glm::min(meshSource->m_BoundingBox.Min.z, min.z);
		//				meshSource->m_BoundingBox.Max.x = glm::max(meshSource->m_BoundingBox.Max.x, max.x);
		//				meshSource->m_BoundingBox.Max.y = glm::max(meshSource->m_BoundingBox.Max.y, max.y);
		//				meshSource->m_BoundingBox.Max.z = glm::max(meshSource->m_BoundingBox.Max.z, max.z);
		//			}
		//		}
		//
		//		// Bones
		//		if (meshSource->HasSkeleton())
		//		{
		//			meshSource->m_BoneInfluences.resize(meshSource->m_Vertices.size());
		//			for (uint32_t m = 0; m < scene->mNumMeshes; m++)
		//			{
		//				aiMesh* mesh = scene->mMeshes[m];
		//				Submesh& submesh = meshSource->m_Submeshes[m];
		//
		//				if (mesh->mNumBones > 0)
		//				{
		//					submesh.IsRigged = true;
		//					for (uint32_t i = 0; i < mesh->mNumBones; i++)
		//					{
		//						aiBone* bone = mesh->mBones[i];
		//						bool hasNonZeroWeight = false;
		//						for (size_t j = 0; j < bone->mNumWeights; j++)
		//						{
		//							if (bone->mWeights[j].mWeight > 0.000001f)
		//							{
		//								hasNonZeroWeight = true;
		//							}
		//						}
		//						if (!hasNonZeroWeight)
		//							continue;
		//
		//						// Find bone in skeleton
		//						uint32_t boneIndex = meshSource->m_Skeleton->GetBoneIndex(bone->mName.C_Str());
		//						if (boneIndex == Skeleton::NullIndex)
		//						{
		//							BEY_CORE_ERROR_TAG("Animation", "Could not find mesh bone '{}' in skeleton!", bone->mName.C_Str());
		//						}
		//
		//						uint32_t boneInfoIndex = ~0;
		//						for (size_t j = 0; j < meshSource->m_BoneInfo.size(); ++j)
		//						{
		//							// note: Same bone could influence different submeshes (and each will have different transforms in the bind pose).
		//							//       Hence the need to differentiate on submesh index here.
		//							if ((meshSource->m_BoneInfo[j].BoneIndex == boneIndex) && (meshSource->m_BoneInfo[j].SubMeshIndex == m))
		//							{
		//								boneInfoIndex = static_cast<uint32_t>(j);
		//								break;
		//							}
		//						}
		//						if (boneInfoIndex == ~0)
		//						{
		//							boneInfoIndex = static_cast<uint32_t>(meshSource->m_BoneInfo.size());
		//							meshSource->m_BoneInfo.emplace_back(glm::inverse(submesh.Transform), Utils::Mat4FromAIMatrix4x4(bone->mOffsetMatrix), m, boneIndex);
		//						}
		//
		//						for (size_t j = 0; j < bone->mNumWeights; j++)
		//						{
		//							int VertexID = submesh.BaseVertex + bone->mWeights[j].mVertexId;
		//							float Weight = bone->mWeights[j].mWeight;
		//							meshSource->m_BoneInfluences[VertexID].AddBoneData(boneInfoIndex, Weight);
		//						}
		//					}
		//				}
		//			}
		//
		//			for (auto& boneInfluence : meshSource->m_BoneInfluences)
		//			{
		//				boneInfluence.NormalizeWeights();
		//			}
		//		}
		//
		//		if (meshSource->m_Vertices.size())
		//			meshSource->m_VertexBuffer = VertexBuffer::Create(meshSource->m_Vertices.data(), (uint32_t)(meshSource->m_Vertices.size() * sizeof(Vertex)), path.string());
		//
		//		if (meshSource->HasSkeleton())
		//		{
		//			meshSource->m_BoneInfluenceBuffer = VertexBuffer::Create(meshSource->m_BoneInfluences.data(), (uint32_t)(meshSource->m_BoneInfluences.size() * sizeof(BoneInfluence)), path.string());
		//		}
		//
		//		if (meshSource->m_Indices.size())
		//			meshSource->m_IndexBuffer = IndexBuffer::Create(meshSource->m_Indices.data(), path.string(), (uint32_t)(meshSource->m_Indices.size() * sizeof(Index)));
		//
		//
		//
		//		// Materials
		//		Ref<Texture2D> whiteTexture = Renderer::GetWhiteTexture();
		//		if (scene->HasMaterials())
		//		{
		//			BEY_MESH_LOG("---- Materials - {0} ----", m_Path);
		//
		//			meshSource->m_Materials.resize(scene->mNumMaterials);
		//
		//			for (uint32_t i = 0; i < scene->mNumMaterials; i++)
		//			{
		//				auto aiMaterial = scene->mMaterials[i];
		//				auto aiMaterialName = aiMaterial->GetName();
		//				Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), aiMaterialName.data));
		//				materialAsset->SetDefaults();
		//				Beyond::Ref<Material> mi = materialAsset->GetMaterial();
		//				meshSource->m_Materials[i] = mi;
		//
		//				BEY_MESH_LOG("  {0} (Index = {1})", aiMaterialName.data, i);
		//
		//				aiString aiTexPath;
		//
		//				glm::vec4 albedoColor(0.8f);
		//				float emission = 0.0f;
		//				float ior = 1.0f;
		//				//int hasTransparency = 0;
		//				aiColor4D aiColor, aiIOR;
		//				if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, aiColor) == AI_SUCCESS)
		//				{
		//					albedoColor = { aiColor.r, aiColor.g, aiColor.b, aiColor.a };
		//					if (aiColor.a < 1.0)
		//						mi->SetFlag(MaterialFlag::Translucent);
		//				}
		//
		//				// Retrieve the emissive factor
		//				bool emissionFallback = false;
		//				if (aiMaterial->Get(AI_MATKEY_GLTF_EMISSIVE_STRENGTH, emission) != AI_SUCCESS)
		//				{
		//					emissionFallback = true;
		//				}
		//
		//				if (emissionFallback)
		//				{
		//					aiColor3D emissiveFactor(0.f, 0.f, 0.f);
		//					if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissiveFactor) == AI_SUCCESS)
		//						emission = glm::dot(glm::vec3{ emissiveFactor.r, emissiveFactor.g, emissiveFactor.b }, glm::vec3{ 0.2126f, 0.7152f, 0.0722f });
		//				}
		//
		//				if (aiMaterial->Get(AI_MATKEY_REFRACTI, ior) == AI_SUCCESS)
		//					ior = aiIOR.r;
		//
		//				//aiColor3D specularColor;
		//				//if (aiMaterial->Get(AI_MATKEY_COLOR_REFLECTIVE, specularColor) == AI_SUCCESS)
		//				//	materialAsset->SetSpecularColor({ specularColor.r, specularColor.g, specularColor.b });
		//
		//				mi->Set("u_MaterialUniforms.AlbedoColor", albedoColor);
		//				//mi->Set("u_MaterialUniforms.Emission", emission);
		//				mi->Set("u_MaterialUniforms.IOR", ior);
		//
		//				float roughness;
		//				float metalness;
		//				if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) != aiReturn_SUCCESS)
		//					roughness = 0.5f; // Default value
		//
		//				if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metalness) != aiReturn_SUCCESS && aiMaterial->Get(AI_MATKEY_REFLECTIVITY, metalness) != aiReturn_SUCCESS)
		//					metalness = 0.0f;
		//
		//				float specular = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_SPECULAR_FACTOR, specular) == AI_SUCCESS)
		//					materialAsset->SetSpecular(specular);
		//
		//				float transmission = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission) == AI_SUCCESS)
		//					materialAsset->SetTransmission(transmission);
		//
		//				float clearCoatFactor = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_CLEARCOAT_FACTOR, clearCoatFactor) == AI_SUCCESS)
		//					materialAsset->SetClearcoat(clearCoatFactor);
		//
		//				float clearCoatRoughness = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_CLEARCOAT_ROUGHNESS_FACTOR, clearCoatRoughness) == AI_SUCCESS)
		//					materialAsset->SetClearcoatRoughness(clearCoatRoughness);
		//
		//				aiColor3D specularColor;
		//				if (aiMaterial->Get(AI_MATKEY_COLOR_SPECULAR, specularColor) == AI_SUCCESS)
		//					materialAsset->SetSpecularColor({ specularColor.r, specularColor.g, specularColor.b });
		//
		//				aiColor3D sheenColor;
		//				if (aiMaterial->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, sheenColor) == AI_SUCCESS)
		//					materialAsset->SetSheenColor({ sheenColor.r, sheenColor.g, sheenColor.b });
		//
		//				float sheenRoughness = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_SHEEN_ROUGHNESS_FACTOR, sheenRoughness) == AI_SUCCESS)
		//					materialAsset->SetSheenRoughness(sheenRoughness);
		//
		//				float thickness = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_VOLUME_THICKNESS_FACTOR, thickness) == AI_SUCCESS)
		//					materialAsset->SetThickness(thickness);
		//
		//				float attenuationDistance = 0.0f;
		//				if (aiMaterial->Get(AI_MATKEY_VOLUME_ATTENUATION_DISTANCE, attenuationDistance) == AI_SUCCESS)
		//					materialAsset->SetAttenuationDistance(attenuationDistance);
		//
		//				aiColor3D attenuationColor;
		//				if (aiMaterial->Get(AI_MATKEY_SHEEN_COLOR_FACTOR, attenuationColor) == AI_SUCCESS)
		//					materialAsset->SetAttenuationColor({ attenuationColor.r, attenuationColor.g, attenuationColor.b });
		//
		//				bool twoSided;
		//				if (aiMaterial->Get(AI_MATKEY_TWOSIDED, twoSided) == AI_SUCCESS)
		//					materialAsset->SetTwoSided(twoSided);
		//
		//
		//
		//
		//				if (!metalnessTextureFound)
		//				{
		//
		//					aiString metalnessTexturePath;
		//					metalnessTextureFound = aiMaterial->GetTexture(aiTextureType_METALNESS, 0, &metalnessTexturePath) == AI_SUCCESS;
		//					fallback = !metalnessTextureFound;
		//					if (metalnessTextureFound)
		//					{
		//
		//						AssetHandle textureHandle = 0;
		//						TextureSpecification spec;
		//						spec.DebugName = metalnessTexturePath.C_Str();
		//						spec.CreateBindlessDescriptor = true;
		//						spec.Compress = true;
		//						spec.UsageType = TextureUsageType::MetalnessRoughness;
		//						if (auto aiTexEmbedded = scene->GetEmbeddedTexture(metalnessTexturePath.C_Str()))
		//						{
		//							//spec.Format = ImageFormat::RGB;
		//							spec.Width = aiTexEmbedded->mWidth;
		//							spec.Height = aiTexEmbedded->mHeight;
		//							spec.DebugName = aiTexEmbedded->mFilename.length ? aiTexEmbedded->mFilename.C_Str() : fmt::eastl_format("Embedded Metalness Tex from: {}", path.string());
		//							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, Buffer(aiTexEmbedded->pcData, 1));
		//						}
		//						else
		//						{
		//							// TODO: Temp - this should be handled by Beyond's filesystem
		//							auto parentPath = path.parent_path();
		//							parentPath /= metalnessTexturePath.C_Str();
		//							std::string texturePath = parentPath.string();
		//							BEY_MESH_LOG("    Metalness map path = {0}", texturePath);
		//							textureHandle = AssetManager::CreateMemoryOnlyRendererAsset<Texture2D>(spec, texturePath);
		//						}
		//
		//						Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(textureHandle);
		//						if (texture && texture->IsStillLoading())
		//						{
		//							metalnessTextureFound = true;
		//							mi->Set("u_MaterialUniforms.MetalnessTexIndex", texture);
		//							mi->Set("u_MaterialUniforms.Metalness", 1.0f);
		//						}
		//						else
		//						{
		//							BEY_CORE_ERROR_TAG("Mesh", "    Could not load texture: {0}", metalnessTexturePath.C_Str());
		//						}
		//					}
		//				}
		//
		//
		//				fallback = !metalnessTextureFound;
		//				if (fallback)
		//				{
		//					BEY_MESH_LOG("    No metalness map");
		//					mi->Set("u_MaterialUniforms.MetalnessTexIndex", whiteTexture);
		//					mi->Set("u_MaterialUniforms.Metalness", metalness);
		//
		//				}
		//			}
		//			BEY_MESH_LOG("------------------------");
		//		}
		//		else
		//		{
		//			if (scene->HasMeshes())
		//			{
		//				Ref<MaterialAsset> materialAsset = Ref<MaterialAsset>::Create(Material::Create(Renderer::GetShaderLibrary()->Get("PBR_Static"), "Beyond-Default"));
		//				materialAsset->SetDefaults();
		//
		//				meshSource->m_Materials.push_back(materialAsset->GetMaterial());
		//			}
		//		}
		Renderer::Submit([meshSource]() mutable
		{
			meshSource->m_IsReady = true;
			meshSource->SetFlag(AssetFlag::StillLoading, false);
		});
		//
		//
		BEY_CORE_INFO("It took {}s to load {}", timer.Elapsed(), m_Path);

		return meshSource;
	}

	void GltfMeshImporter::TraverseNodes(Ref<MeshSource> meshSource, const tinygltf::Model& model, const tinygltf::Node* gltfNode, uint32_t nodeIndex,
		const glm::mat4& parentTransform, uint32_t level)
	{

		tinygltf::Node* aNode = (tinygltf::Node*)gltfNode;

		MeshNode& node = meshSource->m_Nodes[nodeIndex];
		node.Name = eastl::string(aNode->name.c_str(), aNode->name.size());;
		node.LocalTransform = Utils::Mat4FromTinyGLTF(aNode->matrix);

		glm::mat4 transform = parentTransform * node.LocalTransform;
		for (uint32_t i = 0; i < model.meshes.size(); i++)
		{
			uint32_t submeshIndex = model.nodes[i].mesh;
			auto& submesh = meshSource->m_Submeshes[submeshIndex];
			submesh.NodeName = eastl::string(aNode->name.c_str(), aNode->name.size());
			submesh.Transform = transform;
			submesh.LocalTransform = node.LocalTransform;

			node.Submeshes.push_back(submeshIndex);
		}

		// BEY_MESH_LOG("{0} {1}", LevelToSpaces(level), node->mName.C_Str());

		uint32_t parentNodeIndex = (uint32_t)meshSource->m_Nodes.size() - 1;
		node.Children.resize(aNode->children.size());
		for (uint32_t i = 0; i < aNode->children.size(); i++)
		{
			MeshNode& child = meshSource->m_Nodes.emplace_back();
			uint32_t childIndex = static_cast<uint32_t>(meshSource->m_Nodes.size()) - 1;
			child.Parent = parentNodeIndex;
			meshSource->m_Nodes[nodeIndex].Children[i] = childIndex;
			TraverseNodes(meshSource, model, &model.nodes[aNode->children[i]], childIndex, transform, level + 1);
		}
	}
#endif

}
