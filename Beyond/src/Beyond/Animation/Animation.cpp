#include "pch.h"
#include "Animation.h"

#include "Beyond/Renderer/Mesh.h"

#include <acl/core/ansi_allocator.h>
#include <acl/compression/compress.h>
#include <acl/decompression/decompress.h>

#include <glm/gtc/type_ptr.hpp>

namespace Beyond {

	namespace Utils {

		acl::iallocator& GetAnimationAllocator()
		{
			static acl::ansi_allocator s_Allocator;
			return s_Allocator;
		}


		struct RootBoneTrackWriter : public acl::track_writer
		{
			RootBoneTrackWriter(glm::vec3& translation, glm::quat& rotation)
				: m_Rotation(rotation)
				, m_Translation(translation)
			{
			}

			static constexpr bool skip_all_scales() { return true; }

			constexpr bool skip_track_rotation(uint32_t track_index) const { return track_index > 0; }
			constexpr bool skip_track_translation(uint32_t track_index) const { return track_index > 0; }
			constexpr bool skip_track_scale(uint32_t track_index) const { return true; }

			void RTM_SIMD_CALL write_rotation(uint32_t /*track_index*/, rtm::quatf_arg0 rotation)
			{
				rtm::quat_store(rotation, glm::value_ptr(m_Rotation));
			}

			void RTM_SIMD_CALL write_translation(uint32_t /*track_index*/, rtm::vector4f_arg0 translation)
			{
				rtm::vector_store3(translation, glm::value_ptr(m_Translation));
			}

		private:
			glm::quat& m_Rotation;
			glm::vec3& m_Translation;
		};

	}


	Animation::Animation(const float duration, const uint32_t numTracks, void* data)
	: m_Data(data)
	, m_Duration(duration)
	, m_NumTracks(numTracks)
	, m_RootTranslationStart(0.0f, 0.0f, 0.0f)
	, m_RootTranslationEnd(0.0f, 0.0f, 0.0f)
	, m_RootRotationStart(1.0f, 0.0f, 0.0f, 0.0f)
	, m_RootRotationEnd(1.0f, 0.0f, 0.0f, 0.0f)
	{
		Utils::RootBoneTrackWriter writeStart(m_RootTranslationStart, m_RootRotationStart);
		Utils::RootBoneTrackWriter writeEnd(m_RootTranslationEnd, m_RootRotationEnd);
		acl::decompression_context<acl::default_transform_decompression_settings> context;
		context.initialize(*static_cast<acl::compressed_tracks*>(data));
		context.seek(0, acl::sample_rounding_policy::none);
		context.decompress_track(0, writeStart);
		context.seek(m_Duration, acl::sample_rounding_policy::none);
		context.decompress_track(0, writeEnd);
	}


	Animation::Animation(Animation&& other)
		: m_Data(other.m_Data)
		, m_Duration(other.m_Duration)
		, m_NumTracks(other.m_NumTracks)
		, m_RootTranslationStart(other.m_RootTranslationStart)
		, m_RootTranslationEnd(other.m_RootTranslationEnd)
		, m_RootRotationStart(other.m_RootRotationStart)
		, m_RootRotationEnd(other.m_RootRotationEnd)
	{
		other.m_Data = nullptr; // we've moved the data, so make sure it doesn't get deleted
	}


	Animation& Animation::operator=(Animation&& other)
	{
		if (this != &other)
		{
			m_Data = other.m_Data;
			m_Duration = other.m_Duration;
			m_NumTracks = other.m_NumTracks;
			m_RootTranslationStart = other.m_RootTranslationStart;
			m_RootTranslationEnd = other.m_RootTranslationEnd;
			m_RootRotationStart = other.m_RootRotationStart;
			m_RootRotationEnd = other.m_RootRotationEnd;
			other.m_Data = nullptr; // we've moved the data, so make sure it doesn't get deleted
		}
		return *this;
	}


	Animation::~Animation()
	{
		if (m_Data)
		{
			acl::iallocator& allocator = Utils::GetAnimationAllocator();
			allocator.deallocate(m_Data, static_cast<acl::compressed_tracks*>(m_Data)->get_size());
		}
	}


	AnimationAsset::AnimationAsset(Ref<MeshSource> animationSource, Ref<MeshSource> skeletonSource, const uint32_t animationIndex, const bool isMaskedRootMotion, const glm::vec3& rootTranslationMask, float rootRotationMask)
	: m_RootTranslationMask(rootTranslationMask)
	, m_RootRotationMask(rootRotationMask)
	, m_AnimationSource(animationSource)
	, m_SkeletonSource(skeletonSource)
	, m_AnimationIndex(animationIndex)
	, m_IsMaskedRootMotion(isMaskedRootMotion)
	{
		BEY_CORE_ASSERT(rootRotationMask == 0.0f || rootRotationMask == 1.0f);
		BEY_CORE_ASSERT(rootTranslationMask.x == 0.0f || rootTranslationMask.x == 1.0f);
		BEY_CORE_ASSERT(rootTranslationMask.y == 0.0f || rootTranslationMask.y == 1.0f);
		BEY_CORE_ASSERT(rootTranslationMask.z == 0.0f || rootTranslationMask.z == 1.0f);
	}


	Ref<MeshSource> AnimationAsset::GetAnimationSource() const
	{
		return m_AnimationSource;
	}


	Ref<MeshSource> AnimationAsset::GetSkeletonSource() const
	{
		return m_SkeletonSource;
	}


	uint32_t AnimationAsset::GetAnimationIndex() const
	{
		return m_AnimationIndex;
	}


	bool AnimationAsset::IsMaskedRootMotion() const
	{
		return m_IsMaskedRootMotion;
	}


	const glm::vec3& AnimationAsset::GetRootTranslationMask() const
	{
		return m_RootTranslationMask;
	}


	float AnimationAsset::GetRootRotationMask() const
	{
		return m_RootRotationMask;
	}


	const Beyond::Animation* AnimationAsset::GetAnimation() const
	{
		if (m_AnimationSource && m_SkeletonSource && m_SkeletonSource->HasSkeleton())
		{
			return m_AnimationSource->GetAnimation(m_AnimationIndex, m_SkeletonSource->GetSkeleton(), m_IsMaskedRootMotion, m_RootTranslationMask, m_RootRotationMask);
		}
		return nullptr;
	}

}
