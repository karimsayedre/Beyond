#pragma once

namespace Beyond {

	template<typename T, typename... Args>
	T& Entity::AddComponent(Args&&... args)
	{
		BEY_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
		return m_Scene->m_Registry.emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
	}

	template<typename T>
	T& Entity::GetComponent()
	{
		BEY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template<typename T>
	const T& Entity::GetComponent() const
	{
		BEY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		return m_Scene->m_Registry.get<T>(m_EntityHandle);
	}

	template<typename... T>
	bool Entity::HasComponent()
	{
		return m_Scene->m_Registry.has<T...>(m_EntityHandle);
	}

	template<typename... T>
	bool Entity::HasComponent() const
	{
		return m_Scene->m_Registry.has<T...>(m_EntityHandle);
	}

	template<typename...T>
	bool Entity::HasAny()
	{
		return m_Scene->m_Registry.any<T...>(m_EntityHandle);
	}

	template<typename...T>
	bool Entity::HasAny() const
	{
		return m_Scene->m_Registry.any<T...>(m_EntityHandle);
	}

	template<typename...T>
	bool Entity::AnyChildHas() const
	{
		for (auto child : Children())
			if (m_Scene->m_Registry.any<T...>(m_Scene->GetEntityWithUUID(child)))
				return true;
		return false;
	}

	template<typename T>
	void Entity::RemoveComponent()
	{
		BEY_CORE_ASSERT(HasComponent<T>(), "Entity doesn't have component!");
		m_Scene->m_Registry.remove<T>(m_EntityHandle);
	}

	template<typename T>
	void Entity::RemoveComponentIfExists()
	{
		m_Scene->m_Registry.remove_if_exists<T>(m_EntityHandle);
	}

}
