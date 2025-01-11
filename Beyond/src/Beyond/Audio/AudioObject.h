#pragma once

#include "Audio.h"

#include "Beyond/Core/UUID.h"

#include "glm/glm.hpp"

namespace Beyond
{
	//? obsolete
#if 0
class AudioObject : public RefCounted
{
public:
	friend class MiniAudioEngine;

	AudioObject(UUID ID);
	AudioObject(UUID ID, const eastl::string& debugName);

	AudioObject(const AudioObject&& other) noexcept;
	AudioObject() = default;
	AudioObject(const AudioObject&) = delete;
	AudioObject& operator=(const AudioObject&) = default;
	AudioObject& operator=(const AudioObject&& other) noexcept;

public:
	void SetTransform(const Audio::Transform& transform);
	Audio::Transform GetTransform() const { return m_Transform; }
	void SetVelocity(const glm::vec3& velocity);
	glm::vec3 GetVelocity() const { return m_Velocity; }

	UUID GetID() const { return m_ID; }
	eastl::string GetDebugName() const { return m_DebugName; }

private:
	UUID m_ID;
	Audio::Transform m_Transform;
	glm::vec3 m_Velocity;
	eastl::string m_DebugName;
	bool m_Released = false;
};
#endif

struct AudioObjectData
{
	Audio::Transform Transform;
	glm::vec3 Velocity;
};

} // namespace Beyond
