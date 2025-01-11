#include "pch.h" 
#include "Mesh.h"

#include "Beyond/Debug/Profiler.h"
#include "Beyond/Math/Math.h"
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Project/Project.h"
#include "Beyond/Asset/AssimpMeshImporter.h"
#include "Beyond/Asset/AssetManager.h"
#include "Beyond/Renderer/BLAS.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_ptr.hpp>

#include "imgui/imgui.h"

#include <filesystem>

namespace Beyond
{

#define MESH_DEBUG_LOG 0
#if MESH_DEBUG_LOG
#define BEY_MESH_LOG(...) BEY_CORE_TRACE_TAG("Mesh", __VA_ARGS__)
#define BEY_MESH_ERROR(...) BEY_CORE_ERROR_TAG("Mesh", __VA_ARGS__)
#else
#define BEY_MESH_LOG(...)
#define BEY_MESH_ERROR(...)
#endif

	////////////////////////////////////////////////////////
	// MeshSource //////////////////////////////////////////
	////////////////////////////////////////////////////////
	MeshSource::MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const glm::mat4& transform, const std::string& name)
		: m_Vertices(vertices), m_Indices(indices), m_FilePath(name)
	{
		// Generate a new asset handle
		Handle = {};

		Submesh submesh;
		submesh.BaseVertex = 0;
		submesh.BaseIndex = 0;
		submesh.IndexCount = (uint32_t)indices.size() * 3u;
		submesh.Transform = transform;
		m_Submeshes.push_back(submesh);

		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), (uint32_t)(m_Vertices.size() * sizeof(Vertex)), m_FilePath);
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), m_FilePath, (uint32_t)(m_Indices.size() * sizeof(Index)));

		//Renderer::Submit([inst = Ref(this)]() mutable
		//{
			m_IsReady = true;
		//});

	}

	MeshSource::MeshSource(const std::vector<Vertex>& vertices, const std::vector<Index>& indices, const std::string& name, const std::vector<Submesh>& submeshes)
		: m_Submeshes(submeshes), m_Vertices(vertices), m_Indices(indices), m_FilePath(name)
	{
		// Generate a new asset handle
		Handle = {};

		m_VertexBuffer = VertexBuffer::Create(m_Vertices.data(), (uint32_t)(m_Vertices.size() * sizeof(Vertex)), m_FilePath);
		m_IndexBuffer = IndexBuffer::Create(m_Indices.data(), m_FilePath, (uint32_t)(m_Indices.size() * sizeof(Index)));
		// TODO: generate bounding box for submeshes, etc.

		//Renderer::Submit([inst = Ref(this)]() mutable
		//{
		//	inst->m_IsReady = true;
		//});
		m_IsReady = true;

	}

	MeshSource::~MeshSource()
	{
	}

	static eastl::string LevelToSpaces(uint32_t level)
	{
		eastl::string result = "";
		for (uint32_t i = 0; i < level; i++)
			result += "--";
		return result;
	}

	void MeshSource::DumpVertexBuffer()
	{
		// TODO: Convert to ImGui
		BEY_MESH_LOG("------------------------------------------------------");
		BEY_MESH_LOG("Vertex Buffer Dump");
		BEY_MESH_LOG("Mesh: {0}", m_FilePath);
		for (size_t i = 0; i < m_Vertices.size(); i++)
		{
			auto& vertex = m_Vertices[i];
			BEY_MESH_LOG("Vertex: {0}", i);
			BEY_MESH_LOG("Position: {0}, {1}, {2}", vertex.Position.x, vertex.Position.y, vertex.Position.z);
			BEY_MESH_LOG("Normal: {0}, {1}, {2}", vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
			BEY_MESH_LOG("Binormal: {0}, {1}, {2}", vertex.Binormal.x, vertex.Binormal.y, vertex.Binormal.z);
			BEY_MESH_LOG("Tangent: {0}, {1}, {2}", vertex.Tangent.x, vertex.Tangent.y, vertex.Tangent.z);
			BEY_MESH_LOG("TexCoord: {0}, {1}", vertex.Texcoord.x, vertex.Texcoord.y);
			BEY_MESH_LOG("--");
		}
		BEY_MESH_LOG("------------------------------------------------------");
	}


	// TODO (0x): this is temporary.. and will eventually be replaced with some kind of skeleton retargeting
	bool MeshSource::IsCompatibleSkeleton(const uint32_t animationIndex, const Skeleton& skeleton) const
	{
		if (!m_Skeleton)
		{
			BEY_CORE_VERIFY(!m_Runtime);
			auto path = Project::GetEditorAssetManager()->GetFileSystemPath(Handle);
			AssimpMeshImporter importer(path);
			return importer.IsCompatibleSkeleton(animationIndex, skeleton);
		}

		return m_Skeleton->GetBoneNames() == skeleton.GetBoneNames();
	}

	std::vector<eastl::string> MeshSource::GetAnimationNames() const
	{
		return m_AnimationNames;
	}

	const Animation* MeshSource::GetAnimation(const uint32_t animationIndex, const Skeleton& skeleton, bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask) const
	{
		// Note: It's possible that the same animation index could be requested but with different root motion parameters.
		//       This is pretty edge-case, and not currently supported!
		if (!m_Animations[animationIndex])
		{
			// Deferred load of animations.
			// We cannot load them earlier (e.g. in MeshSource constructor) for two reasons:
			// 1) Assimp does not import bones (and hence no skeleton) if the mesh source file contains only animations (and no skin)
			//    This means we need to wait until we know what the skeleton is before we can load the animations.
			// 2) We don't have any way to pass the root motion parameters to the mesh source constructor

			BEY_CORE_VERIFY(!m_Runtime);
			auto path = Project::GetEditorAssetManager()->GetFileSystemPath(Handle);
			AssimpMeshImporter importer(path);
			importer.ImportAnimation(animationIndex, skeleton, isMaskedRootMotion, rootTranslationMask, rootRotationMask, m_Animations[animationIndex]);
		}

		BEY_CORE_ASSERT(animationIndex < m_Animations.size(), "Animation index out of range!");
		BEY_CORE_ASSERT(m_Animations[animationIndex], "Attempted to access null animation!");
		if (animationIndex < m_Animations.size())
		{
			return m_Animations[animationIndex].get();
		}
		return nullptr;
	}


	Mesh::Mesh(Ref<MeshSource> meshSource, const std::string& name)
		: m_MeshSource(meshSource)
	{
		// Generate a new asset handle
		Handle = {};

		m_MeshSource->SetFilePath(name);
		SetSubmeshes({});

		const auto& meshMaterials = meshSource->GetMaterials();
		m_Materials = Ref<MaterialTable>::Create((uint32_t)meshMaterials.size());
		for (size_t i = 0; i < meshMaterials.size(); i++)
			m_Materials->SetMaterial((uint32_t)i, AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(meshMaterials[i]));

		Renderer::Submit([inst = Ref(this)]() mutable
		{
			inst->m_IsReady = true;
		});
	}

	Mesh::Mesh(Ref<MeshSource> meshSource, const std::string& name, const std::vector<uint32_t>& submeshes)
		: m_MeshSource(meshSource)
	{
		// Generate a new asset handle
		Handle = {};

		SetSubmeshes(submeshes);
		m_MeshSource->SetFilePath(name);

		const auto& meshMaterials = meshSource->GetMaterials();
		m_Materials = Ref<MaterialTable>::Create((uint32_t)meshMaterials.size());
		for (size_t i = 0; i < meshMaterials.size(); i++)
			m_Materials->SetMaterial((uint32_t)i, AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(meshMaterials[i]));

		Renderer::Submit([inst = Ref(this)]() mutable
		{
			inst->m_IsReady = true;
		});
	}

	Mesh::Mesh(const Ref<Mesh>& other)
		: m_MeshSource(other->m_MeshSource), m_Materials(other->m_Materials)
	{
		SetSubmeshes(other->m_Submeshes);

	}

	Mesh::~Mesh()
	{
	}

	void Mesh::SetSubmeshes(const std::vector<uint32_t>& submeshes)
	{
		if (!submeshes.empty())
		{
			m_Submeshes = submeshes;
		}
		else
		{
			const auto& submeshes = m_MeshSource->GetSubmeshes();
			m_Submeshes.resize(submeshes.size());
			for (uint32_t i = 0; i < submeshes.size(); i++)
				m_Submeshes[i] = i;
		}
	}

	////////////////////////////////////////////////////////
	// StaticMesh //////////////////////////////////////////
	////////////////////////////////////////////////////////

	StaticMesh::StaticMesh(Ref<MeshSource> meshSource, const std::string& name)
		: m_MeshSource(meshSource)
	{
		// Generate a new asset handle
		Handle = {};

		SetSubmeshes({});
		m_MeshSource->SetFilePath(name);

		const auto& meshMaterials = meshSource->GetMaterials();
		uint32_t numMaterials = static_cast<uint32_t>(meshMaterials.size());
		m_Materials = Ref<MaterialTable>::Create(numMaterials);
		for (uint32_t i = 0; i < numMaterials; i++)
			m_Materials->SetMaterial(i, AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(meshMaterials[i]));

		Renderer::Submit([inst = Ref(this)]() mutable
		{
			inst->m_IsReady = true;
		});
	}

	StaticMesh::StaticMesh(Ref<MeshSource> meshSource, const std::string& name, const std::vector<uint32_t>& submeshes)
		: m_MeshSource(meshSource)
	{
		// Generate a new asset handle
		Handle = {};

		SetSubmeshes(submeshes);
		m_MeshSource->SetFilePath(name);

		const auto& meshMaterials = meshSource->GetMaterials();
		uint32_t numMaterials = static_cast<uint32_t>(meshMaterials.size());
		m_Materials = Ref<MaterialTable>::Create(numMaterials);
		for (uint32_t i = 0; i < numMaterials; i++)
			m_Materials->SetMaterial(i, AssetManager::CreateMemoryOnlyAsset<MaterialAsset>(meshMaterials[i]));

		Renderer::Submit([inst = Ref(this)]() mutable
		{
			inst->m_IsReady = true;
		});
	}

	StaticMesh::StaticMesh(const Ref<StaticMesh>& other)
		: m_MeshSource(other->m_MeshSource), m_Materials(other->m_Materials)
	{
		SetSubmeshes(other->m_Submeshes);
	}

	StaticMesh::~StaticMesh()
	{
	}

	void StaticMesh::SetSubmeshes(const std::vector<uint32_t>& submeshes)
	{
		if (!submeshes.empty())
		{
			m_Submeshes = submeshes;
		}
		else
		{
			const auto& submeshes = m_MeshSource->GetSubmeshes();
			m_Submeshes.resize(submeshes.size());
			for (uint32_t i = 0; i < submeshes.size(); i++)
				m_Submeshes[i] = i;
		}
	}
}
