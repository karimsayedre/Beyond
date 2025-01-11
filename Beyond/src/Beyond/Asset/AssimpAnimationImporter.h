#pragma once

#include "Beyond/Animation/Animation.h"
#include "Beyond/Animation/Skeleton.h"
#include "Beyond/Core/Base.h"

#include <assimp/scene.h>

#include <string_view>
#include <vector>

namespace Beyond::AssimpAnimationImporter {

#ifdef BEY_DIST
	Scope<Skeleton> ImportSkeleton(const eastl::string_view filename) { return nullptr; }
	Scope<Skeleton> ImportSkeleton(const aiScene* scene) { return nullptr; }

	std::vector<eastl::string> GetAnimationNames(const aiScene* scene) { return std::vector<eastl::string>(); }
	Scope<Animation> ImportAnimation(const aiScene* scene, const eastl::string_view animationName, const Skeleton& skeleton, const bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask) { return nullptr; }
#else
	Scope<Skeleton> ImportSkeleton(const eastl::string_view filename);
	Scope<Skeleton> ImportSkeleton(const aiScene* scene);

	std::vector<eastl::string> GetAnimationNames(const aiScene* scene);
	Scope<Animation> ImportAnimation(const aiScene* scene, const uint32_t animationIndex, const Skeleton& skeleton, const bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask);
#endif
}
