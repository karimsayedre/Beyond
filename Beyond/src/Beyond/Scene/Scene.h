#pragma once

#include "Entity.h"

#include "Beyond/Core/TimeStep.h"
#include "Beyond/Core/UUID.h"

#include "Beyond/Editor/EditorCamera.h"
#include "Beyond/Renderer/Mesh.h"

#include "Beyond/Renderer/SceneEnvironment.h"
#include "rtxgi/ddgi/DDGIVolume.h"

namespace Beyond {
	class Mesh;

	namespace AnimationGraph {
		struct AnimationGraph;
	}

	class SceneRenderer;
	class Renderer2D;
	class Prefab;
	class PhysicsScene;

	struct DirectionalLight
	{
		glm::vec3 Direction{};
		float Intensity{};

		glm::vec3 Color{};
		float LightSize{};

		bool SoftShadows{};
		char Padding0[3] { 0, 0, 0 };
		bool CastShadows{};
		char Padding1[3] { 0, 0, 0 };
		float ShadowAmount{};
		float Padding2{};
	};

	struct PointLight
	{
		glm::vec3 Position{};
		float Intensity = 1.0f;

		glm::vec3 Color = glm::vec3(1.0f);
		float Radius = 25.0f;

		float Falloff = 1.f;
		float SourceSize = 0.1f;
		bool CastsShadows = true;
		char Padding0[3]{ 0, 0, 0 };
		bool SoftShadows = true;
		char Padding1[3]{ 0, 0, 0 };
	};

	struct SpotLight
	{
		glm::vec3 Position{};
		float Intensity = 1.0f;

		glm::vec3 Color = glm::vec3(1.0f);
		float Range = 10.0f;

		glm::vec3 Direction{};
		float Falloff = 1.f;

		glm::vec3 Padding3{};
		float SourceSize = 0.1f;

		float Angle = 0.0f;
		float AngleAttenuation = 0.0f;

		bool CastsShadows = true;
		char Padding0[3]{ 0, 0, 0 };

		bool SoftShadows = true;
		char Padding1[3]{ 0, 0, 0 };
	};

	struct LightEnvironment
	{
		static constexpr size_t MaxDirectionalLights = 4;

		DirectionalLight DirectionalLights[MaxDirectionalLights];
		std::vector<PointLight> PointLights;
		std::vector<SpotLight> SpotLights;
		std::vector<rtxgi::DDGIVolumeDesc> DDGIVolumes;
		[[nodiscard]] uint32_t GetPointLightsSize() const { return (uint32_t)(PointLights.size() * sizeof(PointLight)); }
		[[nodiscard]] uint32_t GetSpotLightsSize() const { return (uint32_t)(SpotLights.size() * sizeof(SpotLight)); }
	};

	using EntityMap = std::unordered_map<UUID, Entity>;

	class PhysicsScene;

	struct SceneSpecification
	{
		eastl::string Name = "UntitledScene";
		bool IsEditorScene = false;
		bool Initalize = true;
	};

	class Scene : public Asset
	{
	public:
		struct PerformanceTimers
		{
			float ScriptUpdate = 0.0f;
			float ScriptLateUpdate = 0.0f;
			float PhysicsStep = 0.0f;
		};
	public:
		Scene(const eastl::string& name = "UntitledScene", bool isEditorScene = false, bool initalize = true);
		~Scene();

		void Init();

		void OnUpdateRuntime(Timestep ts);
		void OnUpdateEditor(Timestep ts);
		void BuildAccelerationStructures();

		void OnRenderRuntime(Ref<SceneRenderer> renderer, Timestep ts);
		void OnRenderEditor(Ref<SceneRenderer> renderer, Timestep ts, const EditorCamera& editorCamera);
		void OnRenderSimulation(Ref<SceneRenderer> renderer, Timestep ts, const EditorCamera& editorCamera);

		void OnAnimationGraphCompiled(AssetHandle AnimationGraphHandle);

		void RenderPhysicsDebug(Ref<SceneRenderer> renderer, bool runtime);

		void OnEvent(Event& e);

		// Runtime
		void OnRuntimeStart();
		void OnRuntimeStop();

		void OnSimulationStart();
		void OnSimulationStop();

		void SetViewportSize(uint32_t width, uint32_t height);

		const Ref<Environment>& GetEnvironment() const { return m_Environment; }


		Entity GetMainCameraEntity();

		float& GetSkyboxLod() { return m_SkyboxLod; }
		float GetSkyboxLod() const { return m_SkyboxLod; }

		std::vector<UUID> GetAllChildren(Entity entity) const;

		Entity CreateEntity(const eastl::string& name = "");
		Entity CreateChildEntity(Entity parent, const eastl::string& name = "");
		Entity CreateEntityWithID(UUID uuid, const std::string& name = "", bool shouldSort = true);
		void SubmitToDestroyEntity(Entity entity);
		void DestroyEntity(Entity entity, bool excludeChildren = false, bool first = true);
		void DestroyEntity(UUID entityID, bool excludeChildren = false, bool first = true);

		void ResetTransformsToMesh(Entity entity, bool resetChildren);

		Entity DuplicateEntity(Entity entity);
		Entity CreatePrefabEntity(Entity entity, Entity parent, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr, const glm::vec3* scale = nullptr);

		Entity Instantiate(Ref<Prefab> prefab, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr, const glm::vec3* scale = nullptr);
		Entity InstantiateChild(Ref<Prefab> prefab, Entity parent, const glm::vec3* translation = nullptr, const glm::vec3* rotation = nullptr, const glm::vec3* scale = nullptr);
		Entity InstantiateMesh(Ref<Mesh> mesh, bool generateColliders);

		std::vector<UUID> FindBoneEntityIds(Entity entity, Entity rootEntity, Ref<Mesh> mesh);
		std::vector<UUID> FindBoneEntityIds(Entity entity, Entity rootEntity, Ref<AnimationGraph::AnimationGraph> anim);

		template<typename... Components>
		auto GetAllEntitiesWith()
		{
			return m_Registry.view<Components...>();
		}

		// return entity with id as specified. entity is expected to exist (runtime error if it doesn't)
		Entity GetEntityWithUUID(UUID id) const;

		// return entity with id as specified, or empty entity if cannot be found - caller must check
		Entity TryGetEntityWithUUID(UUID id) const;

		// return entity with tag as specified, or empty entity if cannot be found - caller must check
		Entity TryGetEntityWithTag(const eastl::string& tag);

		// return descendant entity with tag as specified, or empty entity if cannot be found - caller must check
		// descendant could be immediate child, or deeper in the hierachy
		Entity TryGetDescendantEntityWithTag(Entity entity, const std::string& tag);

		void ConvertToLocalSpace(Entity entity);
		void ConvertToWorldSpace(Entity entity);
		glm::mat4 GetWorldSpaceTransformMatrix(Entity entity);
		TransformComponent GetWorldSpaceTransform(Entity entity);

		void ParentEntity(Entity entity, Entity parent);
		void UnparentEntity(Entity entity, bool convertToWorldSpace = true);

		void CopyTo(Ref<Scene>& target);

		UUID GetUUID() const { return m_SceneID; }

		static Ref<Scene> GetScene(UUID uuid);

		bool IsEditorScene() const { return m_IsEditorScene; }
		bool IsPlaying() const { return m_IsPlaying; }

		Ref<PhysicsScene> GetPhysicsScene() const;

		void OnSceneTransition(AssetHandle scene);

		float GetTimeScale() const { return m_TimeScale; }
		void SetTimeScale(float timeScale) { m_TimeScale = timeScale; }

		static AssetType GetStaticType() { return AssetType::Scene; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }

		const eastl::string& GetName() const { return m_Name; }
		void SetName(const eastl::string& name) { m_Name = name; }

		void SetSceneTransitionCallback(const std::function<void(AssetHandle)>& callback) { m_OnSceneTransitionCallback = callback; }
		void SetEntityDestroyedCallback(const std::function<void(Entity)>& callback) { m_OnEntityDestroyedCallback = callback; }

		uint32_t GetViewportWidth() const { return m_ViewportWidth; }
		uint32_t GetViewportHeight() const { return m_ViewportHeight; }

		const EntityMap& GetEntityMap() const { return m_EntityIDMap; }
		std::unordered_set<AssetHandle> GetAssetList();

		const PerformanceTimers& GetPerformanceTimers() const { return m_PerformanceTimers; }

		template<typename TComponent>
		void CopyComponentIfExists(entt::entity dst, entt::registry& dstRegistry, entt::entity src)
		{
			if (m_Registry.has<TComponent>(src))
			{
				auto& srcComponent = m_Registry.get<TComponent>(src);
				dstRegistry.emplace_or_replace<TComponent>(dst, srcComponent);
			}
		}

		template<typename TComponent>
		static void CopyComponentFromScene(Entity dst, Ref<Scene> dstScene, Entity src, Ref<Scene> srcScene)
		{
			srcScene->CopyComponentIfExists<TComponent>((entt::entity)dst, dstScene->m_Registry, (entt::entity)src);
		}

		void DuplicateAnimationInstance(Entity dst, Entity src);

	public:
		static Ref<Scene> CreateEmpty();

	private:
		void OnAudioComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnAudioComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnMeshColliderComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnMeshColliderComponentDestroy(entt::registry& registry, entt::entity entity);

		void OnRigidBodyComponentConstruct(entt::registry& registry, entt::entity entity);
		void OnRigidBodyComponentDestroy(entt::registry& registry, entt::entity entity);
		void OnRigidBodyComponentDestroy_ProEdition(Entity entity);

		void BuildMeshEntityHierarchy(Entity parent, Ref<Mesh> mesh, const MeshNode& node, bool generateColliders);
		void BuildBoneEntityIds(Entity entity);
		void BuildMeshBoneEntityIds(Entity entity, Entity rootEntity);
		void BuildAnimationBoneEntityIds(Entity entity, Entity rootEntity);

		void SortEntities();

		template<typename Fn>
		void SubmitPostUpdateFunc(Fn&& func)
		{
			m_PostUpdateQueue.emplace_back(func);
		}

		std::vector<glm::mat4> GetModelSpaceBoneTransforms(const std::vector<UUID>& boneEntityIds, Ref<Mesh> mesh);
		void UpdateAnimation(Timestep ts, bool isRuntime);

	private:
		UUID m_SceneID;
		entt::entity m_SceneEntity = entt::null;
		entt::registry m_Registry;

		std::function<void(AssetHandle)> m_OnSceneTransitionCallback;
		std::function<void(Entity)> m_OnEntityDestroyedCallback;

		eastl::string m_Name;
		bool m_IsEditorScene = false;
		uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

		EntityMap m_EntityIDMap;


		LightEnvironment m_LightEnvironment;

		Ref<Environment> m_Environment;
		float m_EnvironmentIntensity = 0.0f;

		std::vector<std::function<void()>> m_PostUpdateQueue;

		float m_SkyboxLod = 1.0f;
		bool m_IsPlaying = false;
		bool m_ShouldSimulate = false;

		float m_TimeScale = 1.0f;

		PerformanceTimers m_PerformanceTimers;

		friend class Entity;
		friend class Prefab;
		friend class Physics2D;
		friend class SceneRenderer;
		friend class SceneSerializer;
		friend class PrefabSerializer;
		friend class SceneHierarchyPanel;
		friend class ECSDebugPanel;
	};

}

#include "EntityTemplates.h"
