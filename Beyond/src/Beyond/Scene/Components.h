#pragma once

#include "Beyond/Animation/AnimationGraph.h"
#include "Beyond/Asset/Asset.h"
#include "Beyond/Core/UUID.h"
#include "Beyond/Math/Math.h"
#include "Beyond/Renderer/MaterialAsset.h"
#include "Beyond/Scene/SceneCamera.h"
#include "Beyond/Script/GCManager.h" // Needed for ScriptComponent::GCHandle
#include "Beyond/Physics/PhysicsTypes.h"
#include "Beyond/Asset/MeshColliderAsset.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include <limits>

#include "Beyond/Renderer/Mesh.h"

// ADDING A NEW COMPONENT
// ----------------------
// If you add a new type of component, there are several pieces of code that need updating:
// 1) Add new component here (obviously).
// 2) If appropriate, update SceneHierarchy panel to render the new component, and to allow new component to be added (via "Add Component" menu).
// 3) Update SceneSerializer to (de)serialize the new component.
// 4) Update Scene::DuplicateEntity() to deal with the new component in whatever way is appropriate.
// 5) ditto Scene::CreatePrefabEntity()
// 6) ditto Prefab::CreatePrefabFromEntity()
// 7) ditto Scene::CopyTo()
// 8) If the component is to be accessible from C# go to https://docs.hazelengine.com/Scripting/Extending/ExposingComponents.html and follow the steps outlined there


namespace Beyond {

	struct IDComponent
	{
		UUID ID = 0;
	};

	struct TagComponent
	{
		std::string Tag;

		TagComponent() = default;
		TagComponent(const TagComponent& other) = default;
		TagComponent(const std::string& tag)
			: Tag(tag)
		{
		}

		TagComponent(const eastl::string& tag)
			: Tag(std::string(tag.c_str(), tag.size()))
		{
		}

		operator std::string& () { return Tag; }
		operator const std::string& () const { return Tag; }
		//operator const eastl::string& () const { return { Tag.c_str(), Tag.size() }; }
		//operator eastl::string() const { return { Tag.data(), Tag.size() }; }
	};

	struct RelationshipComponent
	{
		UUID ParentHandle = 0;
		std::vector<UUID> Children;

		RelationshipComponent() = default;
		RelationshipComponent(const RelationshipComponent& other) = default;
		RelationshipComponent(UUID parent)
			: ParentHandle(parent)
		{
		}
	};

	struct PrefabComponent
	{
		UUID PrefabID = 0;
		UUID EntityID = 0;
	};

	struct DDGIVolumeComponent
	{
		bool Enable = true;
		uint32_t           Index = 0;
		uint32_t           RngSeed = 0;

		bool               InsertPerfMarkers = true;
		bool               ShowProbes = true;
		bool               ClearProbes = true;
		bool               ProbeRelocationEnabled = false;
		bool               ProbeClassificationEnabled = false;
		bool               ProbeVariabilityEnabled = false;
		bool               InfiniteScrollingEnabled = false;
		bool               ClearProbeVariability = false;

		glm::vec3  ProbeSpacing{ 0.3f };

		glm::ivec3    ProbeCounts{ 9 };

		int           ProbeNumRays = 256;
		int           ProbeNumIrradianceTexels = 8;
		int           ProbeNumDistanceTexels = 16;

		float              ProbeHysteresis = 0.97f;
		float              ProbeMaxRayDistance = 10.0f;
		float              ProbeNormalBias = 0.02f;
		float              ProbeViewBias = 0.1f;
		float              ProbeIrradianceThreshold = 0.2f;
		float              ProbeBrightnessThreshold = 1.0f;
		float              ProbeVariabilityThreshold = 0.f;

		float              ProbeMinFrontfaceDistance = 0.1f;

		float ProbeDistanceExponent = 50.0f;


		// Irradiance blending happens in post-tonemap space.
		float           ProbeIrradianceEncodingGamma = 5.f;

		// Probe blending assumes probes with more than this ratio of backface hits are inside of geometry
		float           ProbeRandomRayBackfaceThreshold = 0.1f;

		// Probe relocation and probe classification assume probes with more than this ratio of backface hits are inside of geometry
		float           ProbeFixedRayBackfaceThreshold = 0.25f;

	};

	struct TransformComponent
	{
		glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
		glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };
	private:
		// These are private so that you are forced to set them via
		// SetRotation() or SetRotationEuler()
		// This avoids situation where one of them gets set and the other is forgotten.
		//
		// Why do we need both a quat and Euler angle representation for rotation?
		// Because Euler suffers from gimbal lock -> rotations should be stored as quaternions.
		//
		// BUT: quaternions are confusing, and humans like to work with Euler angles.
		// We cannot store just the quaternions and translate to/from Euler because the conversion
		// Euler -> quat -> Euler is not invariant.
		//
		// It's also sometimes useful to be able to store rotations > 360 degrees which
		// quats do not support.
		//
		// Accordingly, we store Euler for "editor" stuff that humans work with, 
		// and quats for everything else.  The two are maintained in-sync via the SetRotation()
		// methods.
		glm::vec3 RotationEuler = { 0.0f, 0.0f, 0.0f };
		glm::quat Rotation = { 1.0f, 0.0f, 0.0f, 0.0f };

	public:
		TransformComponent() = default;
		TransformComponent(const TransformComponent& other) = default;
		TransformComponent(const glm::vec3& translation)
			: Translation(translation)
		{
		}

		glm::mat4 GetTransform() const
		{
			return glm::translate(glm::mat4(1.0f), Translation)
				* glm::toMat4(Rotation)
				* glm::scale(glm::mat4(1.0f), Scale);
		}

		void SetTransform(const glm::mat4& transform)
		{
			Math::DecomposeTransform(transform, Translation, Rotation, Scale);
			RotationEuler = glm::eulerAngles(Rotation);
		}

		glm::vec3 GetRotationEuler() const
		{
			return RotationEuler;
		}

		void SetRotationEuler(const glm::vec3& euler)
		{
			RotationEuler = euler;
			Rotation = glm::quat(RotationEuler);
		}

		glm::quat GetRotation() const
		{
			return Rotation;
		}

		void SetRotation(const glm::quat& quat)
		{
			auto originalEuler = RotationEuler;
			Rotation = quat;
			RotationEuler = glm::eulerAngles(Rotation);

			// Attempt to avoid 180deg flips in the Euler angles when we SetRotation(quat)
			if (
				(fabs(RotationEuler.x - originalEuler.x) == glm::pi<float>()) &&
				(fabs(RotationEuler.z - originalEuler.z) == glm::pi<float>())
			)
			{
				RotationEuler.x = originalEuler.x;
				RotationEuler.y = glm::pi<float>() - RotationEuler.y;
				RotationEuler.z = originalEuler.z;
			}
		}

		friend class SceneSerializer;
	};

	struct MeshComponent
	{
		AssetHandle MeshAssetHandle;
		uint32_t SubmeshIndex = 0;
		Ref<Beyond::MaterialTable> MaterialTable = Ref<Beyond::MaterialTable>::Create();
		std::vector<UUID> BoneEntityIds; // If mesh is rigged, these are the entities whose transforms will used to "skin" the rig.
		bool Visible = true;

		MeshComponent() = default;
		MeshComponent(const MeshComponent& other)
			: MeshAssetHandle(other.MeshAssetHandle), SubmeshIndex(other.SubmeshIndex), MaterialTable(Ref<Beyond::MaterialTable>::Create(other.MaterialTable)), BoneEntityIds(other.BoneEntityIds)
		{
		}
		MeshComponent(AssetHandle mesh, uint32_t submeshIndex = 0)
			: MeshAssetHandle(mesh), SubmeshIndex(submeshIndex)
		{
		}
	};

	struct StaticMeshComponent
	{
		AssetHandle StaticMeshAssetHandle;
		Ref<Beyond::MaterialTable> MaterialTable = Ref<Beyond::MaterialTable>::Create();
		bool Visible = true;

		StaticMeshComponent() = default;
		StaticMeshComponent(const StaticMeshComponent& other)
			: StaticMeshAssetHandle(other.StaticMeshAssetHandle), MaterialTable(Ref<Beyond::MaterialTable>::Create(other.MaterialTable)), Visible(other.Visible)
		{
		}
		StaticMeshComponent(AssetHandle staticMesh)
			: StaticMeshAssetHandle(staticMesh)
		{
		}
	};

	struct AnimationComponent
	{
		AssetHandle AnimationGraphHandle;
		std::vector<UUID> BoneEntityIds; // AnimationGraph refers to a skeleton.  Skeleton has a collection of bones.  Each bone affects the transform of an entity. These are those entities.
		Ref<AnimationGraph::AnimationGraph> AnimationGraph;

		// Note: generally if you copy an AnimationComponent, then you will need to:
		// a) Reset the bone entity ids (e.g.to point to copied entities that the copied component belongs to).  See Scene::DuplicateEntity()
		// b) Create a new independent AnimationGraph instance.  See Scene::DuplicateEntity()
	};

	struct ScriptComponent
	{
		AssetHandle ScriptClassHandle = 0;
		GCHandle ManagedInstance = nullptr;
		std::vector<uint32_t> FieldIDs;

		// NOTE: Get's set to true when OnCreate has been called for this entity
		bool IsRuntimeInitialized = false;

		ScriptComponent() = default;
		ScriptComponent(const ScriptComponent& other) = default;
		ScriptComponent(AssetHandle scriptClassHandle)
			: ScriptClassHandle(scriptClassHandle)
		{
		}
	};

	struct CameraComponent
	{
		enum class Type { None = -1, Perspective, Orthographic };
		Type ProjectionType;

		SceneCamera Camera;
		bool Primary = true;

		CameraComponent() = default;
		CameraComponent(const CameraComponent& other) = default;

		operator SceneCamera& () { return Camera; }
		operator const SceneCamera& () const { return Camera; }
	};

	struct SpriteRendererComponent
	{
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		AssetHandle Texture = 0;
		float TilingFactor = 1.0f;
		glm::vec2 UVStart{ 0.0f, 0.0f };
		glm::vec2 UVEnd{ 1.0f, 1.0f };

		SpriteRendererComponent() = default;
		SpriteRendererComponent(const SpriteRendererComponent& other) = default;
	};

	struct TextComponent
	{
		std::string TextString = "";
		size_t TextHash;

		// Font
		AssetHandle FontHandle;
		glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float LineSpacing = 0.0f;
		float Kerning = 0.0f;

		// Layout
		float MaxWidth = 10.0f;

		bool ScreenSpace = false;
		bool DropShadow = false;
		float ShadowDistance = 0.0f;
		glm::vec4 ShadowColor = { 0.0f, 0.0f, 0.0f, 1.0f };

		TextComponent() = default;
		TextComponent(const TextComponent& other) = default;
	};


	struct RigidBodyComponent
	{
		EBodyType BodyType = EBodyType::Static;
		uint32_t LayerID = 0;
		bool EnableDynamicTypeChange = false;

		float Mass = 1.0f;
		float LinearDrag = 0.01f;
		float AngularDrag = 0.05f;
		bool DisableGravity = false;
		bool IsTrigger = false;
		ECollisionDetectionType CollisionDetection = ECollisionDetectionType::Discrete;

		glm::vec3 InitialLinearVelocity = glm::vec3(0.0f);
		glm::vec3 InitialAngularVelocity = glm::vec3(0.0f);

		float MaxLinearVelocity = 500.0f;
		float MaxAngularVelocity = 50.0f;

		EActorAxis LockedAxes = EActorAxis::None;

		RigidBodyComponent() = default;
		RigidBodyComponent(const RigidBodyComponent& other) = default;
	};

	// A physics actor specifically tailored to character movement
	// For now we support capsule character volume only
	struct CharacterControllerComponent
	{
		float SlopeLimitDeg;
		float StepOffset;
		uint32_t LayerID = 0;
		bool DisableGravity = false;
	};

	// Fixed Joints restricts an object's movement to be dependent upon another object.
	// This is somewhat similar to Parenting but is implemented through physics rather than Transform hierarchy
	struct FixedJointComponent
	{
		UUID ConnectedEntity;

		bool IsBreakable = true;
		float BreakForce = 100.0f;
		float BreakTorque = 10.0f;

		bool EnableCollision = false;
		bool EnablePreProcessing = true;
	};

	struct CompoundColliderComponent
	{
		bool IncludeStaticChildColliders = true;
		bool IsImmutable = true;

		std::vector<UUID> CompoundedColliderEntities;
	};

	struct BoxColliderComponent
	{
		glm::vec3 HalfSize = { 0.5f, 0.5f, 0.5f };
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		ColliderMaterial Material;
	};

	struct SphereColliderComponent
	{
		float Radius = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		ColliderMaterial Material;
	};

	struct CapsuleColliderComponent
	{
		float Radius = 0.5f;
		float HalfHeight = 0.5f;
		glm::vec3 Offset = { 0.0f, 0.0f, 0.0f };

		ColliderMaterial Material;
	};

	struct MeshColliderComponent
	{
		AssetHandle ColliderAsset = 0;
		uint32_t SubmeshIndex = 0;
		bool UseSharedShape = false;
		ColliderMaterial Material;
		ECollisionComplexity CollisionComplexity = ECollisionComplexity::Default;

		MeshColliderComponent() = default;
		MeshColliderComponent(const MeshColliderComponent& other) = default;
		MeshColliderComponent(AssetHandle colliderAsset, uint32_t submeshIndex = 0)
			: ColliderAsset(colliderAsset), SubmeshIndex(submeshIndex)
		{
		}
	};

	enum class LightType
	{
		None = 0, Directional = 1, Point = 2, Spot = 3
	};

	struct DirectionalLightComponent // 6 Params
	{
		glm::vec3 Radiance{ 0.92f, 0.91f, 0.99f };
		float Intensity = 1.0f;
		bool SoftShadows = true;
		bool CastShadows = true;
		float SourceSize = 0.75f;
		float ShadowAmount = 1.0f;
	};

	struct PointLightComponent // 7 Params
	{
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float SourceSize = 0.1f; // For PCSS
		float Radius = 10.f;
		bool CastShadows = true;
		bool SoftShadows = true;
		float Falloff = 1.f;
	};

	struct SpotLightComponent // 9 Params
	{
		glm::vec3 Radiance = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		float SourceSize = 0.1f; // For PCSS
		float Range = 10.f;
		bool CastShadows = true;
		bool SoftShadows = true;
		float Falloff = 1.f;
		float Angle = 45.f;
		float AngleAttenuation = 5.0f;
	};

	struct SkyLightComponent
	{
		AssetHandle SceneEnvironment;
		float Intensity = 1.0f;
		float Lod = 0.0f;

		bool DynamicSky = false;
		glm::vec3 TurbidityAzimuthInclination = { 2.0, 0.0, 0.0 };
	};

	struct AudioListenerComponent
	{
		//int ListenerID = -1;
		bool Active = false;
		float ConeInnerAngleInRadians = 6.283185f; /* 360 degrees. */;
		float ConeOuterAngleInRadians = 6.283185f; /* 360 degrees. */;
		float ConeOuterGain = 0.0f;
		AudioListenerComponent() = default;
		AudioListenerComponent(const AudioListenerComponent& other) = default;
	};

}
