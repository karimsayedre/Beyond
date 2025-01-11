#pragma once
//#include <tinygltf/tiny_gltf.h>
#include "Beyond/Renderer/Mesh.h"

namespace Beyond {

#if 0
	class GltfMeshImporter
	{
	public:
		GltfMeshImporter(const std::filesystem::path& path);

		Ref<MaterialAsset> CreateMaterialFromGLTF(const tinygltf::Model& model, const tinygltf::Material& material);
		Ref<MeshSource> ImportToMeshSource();
		//bool ImportSkeleton(Scope<Skeleton>& skeleton);
		//bool ImportAnimation(const uint32_t animationIndex, const Skeleton& skeleton, const bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask, Scope<Animation>& animation);
		//bool IsCompatibleSkeleton(const uint32_t animationIndex, const Skeleton& skeleton);
		//uint32_t GetAnimationCount();
	private:
		static void TraverseNodes(Ref<MeshSource> meshSource, const tinygltf::Model& model, const tinygltf::Node* gltfNode, uint32_t nodeIndex, const glm::mat4& parentTransform = glm::mat4(1.0f), uint32_t level = 0);
	private:
		const std::filesystem::path m_Path;
	};
#endif

}
