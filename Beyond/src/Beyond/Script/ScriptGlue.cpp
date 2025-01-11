#include "pch.h"
#include "ScriptGlue.h"
#include "ScriptEngine.h"
#include "ScriptUtils.h"
#include "ScriptCache.h"
#include "CSharpInstanceInspector.h"
#include "CSharpInstance.h"

#include "Beyond/Animation/AnimationGraph.h"

#include "Beyond/ImGui/ImGui.h"
#include "Beyond/Core/Events/EditorEvents.h"

#include "Beyond/Asset/AssetManager.h"

#include "Beyond/Audio/AudioEngine.h"
#include "Beyond/Audio/AudioComponent.h"
#include "Beyond/Audio/AudioPlayback.h"
#include "Beyond/Audio/AudioEvents/AudioCommandRegistry.h"

#include "Beyond/Core/Application.h"
#include "Beyond/Core/Hash.h"
#include "Beyond/Core/Math/Noise.h"

#include "Beyond/Renderer/SceneRenderer.h"
#include "Beyond/Physics/PhysicsScene.h"
#include "Beyond/Physics/PhysicsLayer.h"

#include "Beyond/Reflection/TypeName.h"

#include "Beyond/Renderer/MeshFactory.h"

#include "Beyond/Scene/Prefab.h"

#include "Beyond/Utilities/TypeInfo.h"


#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>

#include <mono/metadata/class.h>
#include <mono/metadata/object.h>
#include <mono/metadata/reflection.h>
#include <mono/metadata/loader.h>
#include <mono/metadata/exception.h>
#include <mono/jit/jit.h>

#include <functional>

namespace Beyond {

#ifdef BEY_PLATFORM_WINDOWS
#define BEY_FUNCTION_NAME __func__
#else
#define BEY_FUNCTION_NAME __FUNCTION__
#endif

#define BEY_ADD_INTERNAL_CALL(icall) mono_add_internal_call("Beyond.InternalCalls::"#icall, (void*)InternalCalls::icall)

#ifdef BEY_DIST
#define BEY_ICALL_VALIDATE_PARAM(param) BEY_CORE_VERIFY(param, "{} called with an invalid value ({}) for parameter '{}'", BEY_FUNCTION_NAME, param, #param)
#define BEY_ICALL_VALIDATE_PARAM_V(param, value) BEY_CORE_VERIFY(param, "{} called with an invalid value ({}) for parameter '{}'.\nStack Trace: {}", BEY_FUNCTION_NAME, value, #param, ScriptUtils::GetCurrentStackTrace())
#else
#define BEY_ICALL_VALIDATE_PARAM(param) do { if (!(param)) { BEY_CONSOLE_LOG_ERROR("{} called with an invalid value ({}) for parameter '{}'", BEY_FUNCTION_NAME, param, #param); } } while(false)
#define BEY_ICALL_VALIDATE_PARAM_V(param, value) do { if (!(param)) { BEY_CONSOLE_LOG_ERROR("{} called with an invalid value ({}) for parameter '{}'.\nStack Trace: {}", BEY_FUNCTION_NAME, value, #param, ScriptUtils::GetCurrentStackTrace()); } } while(false)
#endif

	std::unordered_map<MonoType*, std::function<void(Entity&)>> s_CreateComponentFuncs;
	std::unordered_map<MonoType*, std::function<bool(Entity&)>> s_HasComponentFuncs;
	std::unordered_map<MonoType*, std::function<void(Entity&)>> s_RemoveComponentFuncs;

	template<typename TComponent>
	static void RegisterManagedComponent()
	{
		// NOTE: Get the demangled type name of TComponent
		const TypeNameString& componentTypeName = TypeInfo<TComponent, true>().Name();
		std::string componentName = fmt::format("Beyond.{}", componentTypeName);

		MonoType* managedType = mono_reflection_type_from_name(componentName.data(), ScriptEngine::GetCoreAssemblyInfo()->AssemblyImage);

		if (managedType)
		{
			s_CreateComponentFuncs[managedType] = [](Entity& entity) { entity.AddComponent<TComponent>(); };
			s_HasComponentFuncs[managedType] = [](Entity& entity) { return entity.HasComponent<TComponent>(); };
			s_RemoveComponentFuncs[managedType] = [](Entity& entity) { entity.RemoveComponent<TComponent>(); };
		}
		else
		{
			BEY_CORE_VERIFY(false, "No C# component class found for {}!", componentName);
		}
	}

	template<typename TComponent>
	static void RegisterManagedComponent(std::function<void(Entity&)>&& addFunction)
	{
		// NOTE: Get the demangled type name of TComponent
		const TypeNameString& componentTypeName = TypeInfo<TComponent, true>().Name();
		std::string componentName = fmt::format("Beyond.{}", componentTypeName);

		MonoType* managedType = mono_reflection_type_from_name(componentName.data(), ScriptEngine::GetCoreAssemblyInfo()->AssemblyImage);

		if (managedType)
		{
			s_CreateComponentFuncs[managedType] = std::move(addFunction);
			s_HasComponentFuncs[managedType] = [](Entity& entity) { return entity.HasComponent<TComponent>(); };
			s_RemoveComponentFuncs[managedType] = [](Entity& entity) { entity.RemoveComponent<TComponent>(); };
		}
		else
		{
			BEY_CORE_VERIFY(false, "No C# component class found for {}!", componentName);
		}
	}

	template<typename... TArgs>
	static void WarnWithTrace(const std::string& inFormat, TArgs&&... inArgs)
	{
		auto stackTrace = ScriptUtils::GetCurrentStackTrace();
		std::string formattedMessage = fmt::format(fmt::runtime(inFormat), std::forward<TArgs>(inArgs)...);
		Log::GetEditorConsoleLogger()->warn("{}\nStack Trace: {}", formattedMessage, stackTrace);
	}

	template<typename... TArgs>
	static void ErrorWithTrace(const std::string& inFormat, TArgs&&... inArgs)
	{
		auto stackTrace = ScriptUtils::GetCurrentStackTrace();
		std::string formattedMessage = fmt::format(fmt::runtime(inFormat), std::forward<TArgs>(inArgs)...);
		Log::GetEditorConsoleLogger()->error("{}\nStack Trace: {}", formattedMessage, stackTrace);
	}

	void ScriptGlue::RegisterGlue()
	{
		if (s_CreateComponentFuncs.size() > 0)
		{
			s_CreateComponentFuncs.clear();
			s_HasComponentFuncs.clear();
			s_RemoveComponentFuncs.clear();
		}

		RegisterComponentTypes();
		RegisterInternalCalls();
	}

	Ref<PhysicsScene> GetPhysicsScene()
	{
		Ref<Scene> entityScene = ScriptEngine::GetSceneContext();

		if (!entityScene->IsPlaying())
			return nullptr;

		return entityScene->GetPhysicsScene();
	}

	void ScriptGlue::RegisterComponentTypes()
	{
		RegisterManagedComponent<TransformComponent>();
		RegisterManagedComponent<TagComponent>();
		RegisterManagedComponent<MeshComponent>();
		RegisterManagedComponent<StaticMeshComponent>();
		RegisterManagedComponent<AnimationComponent>();
		RegisterManagedComponent<ScriptComponent>();
		RegisterManagedComponent<CameraComponent>();
		RegisterManagedComponent<DirectionalLightComponent>();
		RegisterManagedComponent<PointLightComponent>();
		RegisterManagedComponent<SpotLightComponent>();
		RegisterManagedComponent<SkyLightComponent>();
		RegisterManagedComponent<SpriteRendererComponent>();
		RegisterManagedComponent<RigidBodyComponent>([](Entity& entity)
		{
			RigidBodyComponent component;
			component.EnableDynamicTypeChange = true;
			entity.AddComponent<RigidBodyComponent>(component);
		});
		RegisterManagedComponent<BoxColliderComponent>();
		RegisterManagedComponent<SphereColliderComponent>();
		RegisterManagedComponent<CapsuleColliderComponent>();
		RegisterManagedComponent<MeshColliderComponent>();
		RegisterManagedComponent<CharacterControllerComponent>();
		RegisterManagedComponent<FixedJointComponent>();
		RegisterManagedComponent<TextComponent>();
		RegisterManagedComponent<AudioListenerComponent>();
		RegisterManagedComponent<AudioComponent>();
	}

	void ScriptGlue::RegisterInternalCalls()
	{
		BEY_ADD_INTERNAL_CALL(AssetHandle_IsValid);

		BEY_ADD_INTERNAL_CALL(Application_Quit);
		BEY_ADD_INTERNAL_CALL(Application_GetWidth);
		BEY_ADD_INTERNAL_CALL(Application_GetHeight);
		BEY_ADD_INTERNAL_CALL(Application_GetDataDirectoryPath);
		BEY_ADD_INTERNAL_CALL(Application_GetSetting);
		BEY_ADD_INTERNAL_CALL(Application_GetSettingInt);
		BEY_ADD_INTERNAL_CALL(Application_GetSettingFloat);

		BEY_ADD_INTERNAL_CALL(SceneManager_IsSceneValid);
		BEY_ADD_INTERNAL_CALL(SceneManager_IsSceneIDValid);
		BEY_ADD_INTERNAL_CALL(SceneManager_LoadScene);
		BEY_ADD_INTERNAL_CALL(SceneManager_LoadSceneByID);
		BEY_ADD_INTERNAL_CALL(SceneManager_GetCurrentSceneID);
		BEY_ADD_INTERNAL_CALL(SceneManager_GetCurrentSceneName);

		BEY_ADD_INTERNAL_CALL(Scene_FindEntityByTag);
		BEY_ADD_INTERNAL_CALL(Scene_IsEntityValid);
		BEY_ADD_INTERNAL_CALL(Scene_CreateEntity);
		BEY_ADD_INTERNAL_CALL(Scene_InstantiatePrefab);
		BEY_ADD_INTERNAL_CALL(Scene_InstantiatePrefabWithTranslation);
		BEY_ADD_INTERNAL_CALL(Scene_InstantiatePrefabWithTransform);
		BEY_ADD_INTERNAL_CALL(Scene_InstantiateChildPrefabWithTranslation);
		BEY_ADD_INTERNAL_CALL(Scene_InstantiateChildPrefabWithTransform);
		BEY_ADD_INTERNAL_CALL(Scene_DestroyEntity);
		BEY_ADD_INTERNAL_CALL(Scene_DestroyAllChildren);
		BEY_ADD_INTERNAL_CALL(Scene_GetEntities);
		BEY_ADD_INTERNAL_CALL(Scene_GetChildrenIDs);
		BEY_ADD_INTERNAL_CALL(Scene_SetTimeScale);

		BEY_ADD_INTERNAL_CALL(Entity_GetParent);
		BEY_ADD_INTERNAL_CALL(Entity_SetParent);
		BEY_ADD_INTERNAL_CALL(Entity_GetChildren);
		BEY_ADD_INTERNAL_CALL(Entity_CreateComponent);
		BEY_ADD_INTERNAL_CALL(Entity_HasComponent);
		BEY_ADD_INTERNAL_CALL(Entity_RemoveComponent);

		BEY_ADD_INTERNAL_CALL(TagComponent_GetTag);
		BEY_ADD_INTERNAL_CALL(TagComponent_SetTag);

		BEY_ADD_INTERNAL_CALL(TransformComponent_GetTransform);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetTransform);
		BEY_ADD_INTERNAL_CALL(TransformComponent_GetTranslation);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetTranslation);
		BEY_ADD_INTERNAL_CALL(TransformComponent_GetRotation);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetRotation);
		BEY_ADD_INTERNAL_CALL(TransformComponent_GetScale);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetScale);
		BEY_ADD_INTERNAL_CALL(TransformComponent_GetWorldSpaceTransform);
		BEY_ADD_INTERNAL_CALL(TransformComponent_GetTransformMatrix);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetTransformMatrix);
		BEY_ADD_INTERNAL_CALL(TransformComponent_SetRotationQuat);
		BEY_ADD_INTERNAL_CALL(TransformMultiply_Native);

		BEY_ADD_INTERNAL_CALL(MeshComponent_GetMesh);
		BEY_ADD_INTERNAL_CALL(MeshComponent_SetMesh);
		BEY_ADD_INTERNAL_CALL(MeshComponent_HasMaterial);
		BEY_ADD_INTERNAL_CALL(MeshComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(MeshComponent_GetIsRigged);

		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_GetMesh);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_SetMesh);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_HasMaterial);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_SetMaterial);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_IsVisible);
		BEY_ADD_INTERNAL_CALL(StaticMeshComponent_SetVisible);

		BEY_ADD_INTERNAL_CALL(Identifier_Get);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_GetInputBool);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_SetInputBool);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_GetInputInt);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_SetInputInt);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_GetInputFloat);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_SetInputFloat);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_GetInputVector3);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_SetInputVector3);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_SetInputTrigger);
		BEY_ADD_INTERNAL_CALL(AnimationComponent_GetRootMotion);

		BEY_ADD_INTERNAL_CALL(ScriptComponent_GetInstance);

		BEY_ADD_INTERNAL_CALL(CameraComponent_SetPerspective);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetOrthographic);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetVerticalFOV);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetVerticalFOV);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveNearClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveNearClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetPerspectiveFarClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetPerspectiveFarClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetOrthographicSize);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetOrthographicSize);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetOrthographicNearClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetOrthographicNearClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetOrthographicFarClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetOrthographicFarClip);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetProjectionType);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetProjectionType);
		BEY_ADD_INTERNAL_CALL(CameraComponent_GetPrimary);
		BEY_ADD_INTERNAL_CALL(CameraComponent_SetPrimary);

		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_GetRadiance);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_SetRadiance);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_GetIntensity);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_SetIntensity);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_GetCastShadows);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_SetCastShadows);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_GetSoftShadows);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_SetSoftShadows);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_GetLightSize);
		BEY_ADD_INTERNAL_CALL(DirectionalLightComponent_SetLightSize);

		BEY_ADD_INTERNAL_CALL(PointLightComponent_GetRadiance);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_SetRadiance);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_GetIntensity);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_SetIntensity);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_GetRadius);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_SetRadius);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_GetFalloff);
		BEY_ADD_INTERNAL_CALL(PointLightComponent_SetFalloff);

		BEY_ADD_INTERNAL_CALL(SkyLightComponent_GetIntensity);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_SetIntensity);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_GetTurbidity);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_SetTurbidity);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_GetAzimuth);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_SetAzimuth);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_GetInclination);
		BEY_ADD_INTERNAL_CALL(SkyLightComponent_SetInclination);
		
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetRadiance);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetRadiance);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetIntensity);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetIntensity);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetRange);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetRange);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetAngle);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetAngle);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetAngleAttenuation);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetAngleAttenuation);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetFalloff);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetFalloff);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetCastsShadows);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetCastsShadows);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_GetSoftShadows);
		BEY_ADD_INTERNAL_CALL(SpotLightComponent_SetSoftShadows);

		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_GetColor);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_SetColor);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_GetTilingFactor);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_SetTilingFactor);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_GetUVStart);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_SetUVStart);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_GetUVEnd);
		BEY_ADD_INTERNAL_CALL(SpriteRendererComponent_SetUVEnd);

		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_AddForce);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_AddForceAtLocation);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_AddTorque);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetLinearVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetLinearVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetAngularVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetAngularVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetMaxLinearVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetMaxLinearVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetMaxAngularVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetMaxAngularVelocity);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetLinearDrag);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetLinearDrag);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetAngularDrag);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetAngularDrag);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_Rotate);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetLayer);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetLayer);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetLayerName);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetLayerByName);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetMass);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetMass);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetBodyType);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetBodyType);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_MoveKinematic);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetAxisLock);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_IsAxisLocked);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_GetLockedAxes);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_IsSleeping);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_SetIsSleeping);
		BEY_ADD_INTERNAL_CALL(RigidBodyComponent_Teleport);

		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_GetIsGravityEnabled);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetIsGravityEnabled);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_GetSlopeLimit);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetSlopeLimit);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_GetStepOffset);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetStepOffset);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetTranslation);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetRotation);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_Move);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_Jump);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_GetLinearVelocity);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_SetLinearVelocity);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_IsGrounded);
		BEY_ADD_INTERNAL_CALL(CharacterControllerComponent_GetCollisionFlags);

		BEY_ADD_INTERNAL_CALL(FixedJointComponent_GetConnectedEntity);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetConnectedEntity);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_IsBreakable);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetIsBreakable);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_IsBroken);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_Break);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_GetBreakForce);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetBreakForce);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_GetBreakTorque);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetBreakTorque);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_IsCollisionEnabled);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetCollisionEnabled);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_IsPreProcessingEnabled);
		BEY_ADD_INTERNAL_CALL(FixedJointComponent_SetPreProcessingEnabled);

		BEY_ADD_INTERNAL_CALL(BoxColliderComponent_GetHalfSize);
		BEY_ADD_INTERNAL_CALL(BoxColliderComponent_GetOffset);
		BEY_ADD_INTERNAL_CALL(BoxColliderComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(BoxColliderComponent_SetMaterial);

		BEY_ADD_INTERNAL_CALL(SphereColliderComponent_GetRadius);
		BEY_ADD_INTERNAL_CALL(SphereColliderComponent_GetOffset);
		BEY_ADD_INTERNAL_CALL(SphereColliderComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(SphereColliderComponent_SetMaterial);

		BEY_ADD_INTERNAL_CALL(CapsuleColliderComponent_GetRadius);
		BEY_ADD_INTERNAL_CALL(CapsuleColliderComponent_GetHeight);
		BEY_ADD_INTERNAL_CALL(CapsuleColliderComponent_GetOffset);
		BEY_ADD_INTERNAL_CALL(CapsuleColliderComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(CapsuleColliderComponent_SetMaterial);

		BEY_ADD_INTERNAL_CALL(MeshColliderComponent_IsMeshStatic);
		BEY_ADD_INTERNAL_CALL(MeshColliderComponent_IsColliderMeshValid);
		BEY_ADD_INTERNAL_CALL(MeshColliderComponent_GetColliderMesh);
		BEY_ADD_INTERNAL_CALL(MeshColliderComponent_GetMaterial);
		BEY_ADD_INTERNAL_CALL(MeshColliderComponent_SetMaterial);

		BEY_ADD_INTERNAL_CALL(MeshCollider_IsStaticMesh);

		BEY_ADD_INTERNAL_CALL(AudioComponent_IsPlaying);
		BEY_ADD_INTERNAL_CALL(AudioComponent_Play);
		BEY_ADD_INTERNAL_CALL(AudioComponent_Stop);
		BEY_ADD_INTERNAL_CALL(AudioComponent_Pause);
		BEY_ADD_INTERNAL_CALL(AudioComponent_Resume);
		BEY_ADD_INTERNAL_CALL(AudioComponent_GetVolumeMult);
		BEY_ADD_INTERNAL_CALL(AudioComponent_SetVolumeMult);
		BEY_ADD_INTERNAL_CALL(AudioComponent_GetPitchMult);
		BEY_ADD_INTERNAL_CALL(AudioComponent_SetPitchMult);
		BEY_ADD_INTERNAL_CALL(AudioComponent_SetEvent);

		BEY_ADD_INTERNAL_CALL(TextComponent_GetHash);
		BEY_ADD_INTERNAL_CALL(TextComponent_GetText);
		BEY_ADD_INTERNAL_CALL(TextComponent_SetText);
		BEY_ADD_INTERNAL_CALL(TextComponent_GetColor);
		BEY_ADD_INTERNAL_CALL(TextComponent_SetColor);

		//============================================================================================
		/// Audio
		BEY_ADD_INTERNAL_CALL(Audio_PostEvent);
		BEY_ADD_INTERNAL_CALL(Audio_PostEventFromAC);
		BEY_ADD_INTERNAL_CALL(Audio_PostEventAtLocation);
		BEY_ADD_INTERNAL_CALL(Audio_StopEventID);
		BEY_ADD_INTERNAL_CALL(Audio_PauseEventID);
		BEY_ADD_INTERNAL_CALL(Audio_ResumeEventID);
		BEY_ADD_INTERNAL_CALL(Audio_CreateAudioEntity);

		BEY_ADD_INTERNAL_CALL(AudioCommandID_Constructor);
		//============================================================================================
		/// Audio Parameters Interface
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterFloat);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterInt);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterBool);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterFloatForAC);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterIntForAC);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterBoolForAC);

		BEY_ADD_INTERNAL_CALL(Audio_SetParameterFloatForEvent);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterIntForEvent);
		BEY_ADD_INTERNAL_CALL(Audio_SetParameterBoolForEvent);
		//============================================================================================
		BEY_ADD_INTERNAL_CALL(Audio_PreloadEventSources);
		BEY_ADD_INTERNAL_CALL(Audio_UnloadEventSources);

		BEY_ADD_INTERNAL_CALL(Audio_SetLowPassFilterValue);
		BEY_ADD_INTERNAL_CALL(Audio_SetHighPassFilterValue);
		BEY_ADD_INTERNAL_CALL(Audio_SetLowPassFilterValue_Event);
		BEY_ADD_INTERNAL_CALL(Audio_SetHighPassFilterValue_Event);

		BEY_ADD_INTERNAL_CALL(Audio_SetLowPassFilterValue_AC);
		BEY_ADD_INTERNAL_CALL(Audio_SetHighPassFilterValue_AC);

		//============================================================================================

		BEY_ADD_INTERNAL_CALL(Texture2D_Create);
		BEY_ADD_INTERNAL_CALL(Texture2D_GetSize);
		BEY_ADD_INTERNAL_CALL(Texture2D_SetData);
		//BEY_ADD_INTERNAL_CALL(Texture2D_GetData);

		BEY_ADD_INTERNAL_CALL(Mesh_GetMaterialByIndex);
		BEY_ADD_INTERNAL_CALL(Mesh_GetMaterialCount);

		BEY_ADD_INTERNAL_CALL(StaticMesh_GetMaterialByIndex);
		BEY_ADD_INTERNAL_CALL(StaticMesh_GetMaterialCount);

		BEY_ADD_INTERNAL_CALL(Material_GetAlbedoColor);
		BEY_ADD_INTERNAL_CALL(Material_SetAlbedoColor);
		BEY_ADD_INTERNAL_CALL(Material_GetMetalness);
		BEY_ADD_INTERNAL_CALL(Material_SetMetalness);
		BEY_ADD_INTERNAL_CALL(Material_GetRoughness);
		BEY_ADD_INTERNAL_CALL(Material_SetRoughness);
		BEY_ADD_INTERNAL_CALL(Material_GetEmission);
		BEY_ADD_INTERNAL_CALL(Material_SetEmission);
		BEY_ADD_INTERNAL_CALL(Material_SetFloat);
		BEY_ADD_INTERNAL_CALL(Material_SetVector3);
		BEY_ADD_INTERNAL_CALL(Material_SetVector4);
		BEY_ADD_INTERNAL_CALL(Material_SetTexture);

		BEY_ADD_INTERNAL_CALL(MeshFactory_CreatePlane);

		BEY_ADD_INTERNAL_CALL(Physics_CastRay);
		BEY_ADD_INTERNAL_CALL(Physics_CastShape);
		BEY_ADD_INTERNAL_CALL(Physics_OverlapShape);
		BEY_ADD_INTERNAL_CALL(Physics_GetGravity);
		BEY_ADD_INTERNAL_CALL(Physics_SetGravity);
		BEY_ADD_INTERNAL_CALL(Physics_AddRadialImpulse);

		BEY_ADD_INTERNAL_CALL(Matrix4_LookAt);

		BEY_ADD_INTERNAL_CALL(Noise_Constructor);
		BEY_ADD_INTERNAL_CALL(Noise_Destructor);
		BEY_ADD_INTERNAL_CALL(Noise_GetFrequency);
		BEY_ADD_INTERNAL_CALL(Noise_SetFrequency);
		BEY_ADD_INTERNAL_CALL(Noise_GetFractalOctaves);
		BEY_ADD_INTERNAL_CALL(Noise_SetFractalOctaves);
		BEY_ADD_INTERNAL_CALL(Noise_GetFractalLacunarity);
		BEY_ADD_INTERNAL_CALL(Noise_SetFractalLacunarity);
		BEY_ADD_INTERNAL_CALL(Noise_GetFractalGain);
		BEY_ADD_INTERNAL_CALL(Noise_SetFractalGain);
		BEY_ADD_INTERNAL_CALL(Noise_Get);

		BEY_ADD_INTERNAL_CALL(Noise_SetSeed);
		BEY_ADD_INTERNAL_CALL(Noise_Perlin);

		BEY_ADD_INTERNAL_CALL(Log_LogMessage);

		BEY_ADD_INTERNAL_CALL(Input_IsKeyPressed);
		BEY_ADD_INTERNAL_CALL(Input_IsKeyHeld);
		BEY_ADD_INTERNAL_CALL(Input_IsKeyDown);
		BEY_ADD_INTERNAL_CALL(Input_IsKeyReleased);
		BEY_ADD_INTERNAL_CALL(Input_IsMouseButtonPressed);
		BEY_ADD_INTERNAL_CALL(Input_IsMouseButtonHeld);
		BEY_ADD_INTERNAL_CALL(Input_IsMouseButtonDown);
		BEY_ADD_INTERNAL_CALL(Input_IsMouseButtonReleased);
		BEY_ADD_INTERNAL_CALL(Input_GetMousePosition);
		BEY_ADD_INTERNAL_CALL(Input_SetCursorMode);
		BEY_ADD_INTERNAL_CALL(Input_GetCursorMode);
		BEY_ADD_INTERNAL_CALL(Input_IsControllerPresent);
		BEY_ADD_INTERNAL_CALL(Input_GetConnectedControllerIDs);
		BEY_ADD_INTERNAL_CALL(Input_GetControllerName);
		BEY_ADD_INTERNAL_CALL(Input_IsControllerButtonPressed);
		BEY_ADD_INTERNAL_CALL(Input_IsControllerButtonHeld);
		BEY_ADD_INTERNAL_CALL(Input_IsControllerButtonDown);
		BEY_ADD_INTERNAL_CALL(Input_IsControllerButtonReleased);
		BEY_ADD_INTERNAL_CALL(Input_GetControllerAxis);
		BEY_ADD_INTERNAL_CALL(Input_GetControllerHat);
		BEY_ADD_INTERNAL_CALL(Input_GetControllerDeadzone);
		BEY_ADD_INTERNAL_CALL(Input_SetControllerDeadzone);

		BEY_ADD_INTERNAL_CALL(SceneRenderer_GetOpacity);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_SetOpacity);

		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_IsEnabled);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_SetEnabled);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_GetFocusDistance);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_SetFocusDistance);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_GetBlurSize);
		BEY_ADD_INTERNAL_CALL(SceneRenderer_DepthOfField_SetBlurSize);

		BEY_ADD_INTERNAL_CALL(DebugRenderer_DrawLine);
		BEY_ADD_INTERNAL_CALL(DebugRenderer_DrawQuadBillboard);
		BEY_ADD_INTERNAL_CALL(DebugRenderer_SetLineWidth);

		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetFrameTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetGPUTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetMainThreadWorkTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetMainThreadWaitTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetRenderThreadWorkTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetRenderThreadWaitTime);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetFramesPerSecond);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetEntityCount);
		BEY_ADD_INTERNAL_CALL(PerformanceTimers_GetScriptEntityCount);

#ifndef BEY_DIST
		// Editor Only
		BEY_ADD_INTERNAL_CALL(EditorUI_Text);
		BEY_ADD_INTERNAL_CALL(EditorUI_Button);
		BEY_ADD_INTERNAL_CALL(EditorUI_BeginPropertyHeader);
		BEY_ADD_INTERNAL_CALL(EditorUI_EndPropertyHeader);
		BEY_ADD_INTERNAL_CALL(EditorUI_PropertyGrid);
		BEY_ADD_INTERNAL_CALL(EditorUI_PropertyFloat);
		BEY_ADD_INTERNAL_CALL(EditorUI_PropertyVec2);
		BEY_ADD_INTERNAL_CALL(EditorUI_PropertyVec3);
		BEY_ADD_INTERNAL_CALL(EditorUI_PropertyVec4);
#endif
	}

	namespace InternalCalls {

		static inline Entity GetEntity(uint64_t entityID)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			return scene->TryGetEntityWithUUID(entityID);
		};

#pragma region AssetHandle

		bool AssetHandle_IsValid(AssetHandle* assetHandle)
		{
			return AssetManager::IsAssetHandleValid(*assetHandle);
		}

#pragma endregion

#pragma region Application

		void Application_Quit()
		{
#ifdef BEY_DIST
			Application::Get().DispatchEvent<WindowCloseEvent>();
#else
			Application::Get().DispatchEvent<EditorExitPlayModeEvent>();
#endif
		}

		uint32_t Application_GetWidth() { return ScriptEngine::GetSceneContext()->GetViewportWidth(); }
		uint32_t Application_GetHeight() { return ScriptEngine::GetSceneContext()->GetViewportHeight(); }

		MonoString* Application_GetSetting(MonoString* name, MonoString* defaultValue)
		{
			eastl::string key = ScriptUtils::MonoStringToUTF8(name);
			eastl::string defaultValueString = ScriptUtils::MonoStringToUTF8(defaultValue);
			eastl::string value = Application::Get().GetSettings().Get(key, defaultValueString);
			return ScriptUtils::UTF8StringToMono(value);
		}

		int Application_GetSettingInt(MonoString* name, int defaultValue)
		{
			eastl::string key = ScriptUtils::MonoStringToUTF8(name);
			return Application::Get().GetSettings().GetInt(key, defaultValue);
		}
		
		float Application_GetSettingFloat(MonoString* name, float defaultValue)
		{
			eastl::string key = ScriptUtils::MonoStringToUTF8(name);
			return Application::Get().GetSettings().GetFloat(key, defaultValue);
		}

		MonoString* Application_GetDataDirectoryPath()
		{
			auto filepath = Project::GetProjectDirectory() / "Data";
			if (!std::filesystem::exists(filepath))
				std::filesystem::create_directory(filepath);

			return ScriptUtils::UTF8StringToMono(filepath.string().c_str());
		}

#pragma endregion

#pragma region SceneManager

		bool SceneManager_IsSceneValid(MonoString* inScene)
		{
			eastl::string sceneFilePath = ScriptUtils::MonoStringToUTF8(inScene);
			return FileSystem::Exists(Project::GetAssetDirectory() / sceneFilePath.c_str());
		}

		bool SceneManager_IsSceneIDValid(uint64_t sceneID)
		{
			return AssetManager::GetAsset<Scene>(sceneID) != nullptr;
		}

		void SceneManager_LoadScene(AssetHandle* sceneHandle)
		{
			Ref<Scene> activeScene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(activeScene, "No active scene!");
			BEY_ICALL_VALIDATE_PARAM_V(sceneHandle, "nullptr");
			BEY_ICALL_VALIDATE_PARAM(AssetManager::IsAssetHandleValid(*sceneHandle));

			activeScene->OnSceneTransition(*sceneHandle);
		}

		void SceneManager_LoadSceneByID(uint64_t sceneID)
		{
			Ref<Scene> activeScene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(activeScene, "No active scene!");

			// TODO: OnSceneTransition should take scene by AssetHandle, NOT filepath (because this won't work in runtime)
			const auto& metadata = Project::GetEditorAssetManager()->GetMetadata(sceneID);

			if (metadata.Type != AssetType::Scene || !metadata.IsValid())
			{
				ErrorWithTrace("Tried to load a scene with an invalid ID ('{}')", sceneID);
				return;
			}

			activeScene->OnSceneTransition(sceneID);
		}

		uint64_t SceneManager_GetCurrentSceneID() { return ScriptEngine::GetSceneContext()->GetUUID(); }

		MonoString* SceneManager_GetCurrentSceneName()
		{ 
			//TODO: It would be good if this could take an AssetHandle and return the name of the specified scene
			//return activeScene = AssetManager::GetAsset<Scene>(assetHandle)->GetName();

			Ref<Scene> activeScene = ScriptEngine::GetSceneContext();
			return ScriptUtils::UTF8StringToMono(activeScene->GetName());
		}

#pragma endregion

#pragma region Scene

		uint64_t Scene_FindEntityByTag(MonoString* tag)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity entity = scene->TryGetEntityWithTag(ScriptUtils::MonoStringToUTF8(tag));
			return entity ? entity.GetUUID() : UUID(0);
		}

		bool Scene_IsEntityValid(uint64_t entityID)
		{
			if (entityID == 0)
				return false;

			return (bool)(ScriptEngine::GetSceneContext()->TryGetEntityWithUUID(entityID));
		}

		uint64_t Scene_CreateEntity(MonoString* tag)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			return scene->CreateEntity(ScriptUtils::MonoStringToUTF8(tag)).GetUUID();
		}

		uint64_t Scene_InstantiatePrefab(AssetHandle* prefabHandle)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");

			Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(*prefabHandle);
			if (prefab == nullptr)
			{
				WarnWithTrace("Cannot instantiate prefab. No prefab with handle {} found.", *prefabHandle);
				return 0;
			}

			return scene->Instantiate(prefab).GetUUID();
		}

		uint64_t Scene_InstantiatePrefabWithTranslation(AssetHandle* prefabHandle, glm::vec3* inTranslation)
		{
			return Scene_InstantiatePrefabWithTransform(prefabHandle, inTranslation, nullptr, nullptr);
		}

		uint64_t Scene_InstantiatePrefabWithTransform(AssetHandle* prefabHandle, glm::vec3* inTranslation, glm::vec3* inRotation, glm::vec3* inScale)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			BEY_ICALL_VALIDATE_PARAM_V(prefabHandle, "nullptr");

			Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(*prefabHandle);
			if (prefab == nullptr)
			{
				WarnWithTrace("Cannot instantiate prefab. No prefab with handle {} found.", *prefabHandle);
				return 0;
			}

			return scene->Instantiate(prefab, inTranslation, inRotation, inScale).GetUUID();
		}

		uint64_t Scene_InstantiateChildPrefabWithTranslation(uint64_t parentID, AssetHandle* prefabHandle, glm::vec3* inTranslation)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity parent = scene->TryGetEntityWithUUID(parentID);
			BEY_ICALL_VALIDATE_PARAM_V(parent, parentID);
			BEY_ICALL_VALIDATE_PARAM_V(prefabHandle, "nullptr");

			Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(*prefabHandle);
			if (prefab == nullptr)
			{
				ErrorWithTrace("Cannot instantiate prefab. No prefab with handle {} found.", *prefabHandle);
				return 0;
			}

			return scene->InstantiateChild(prefab, parent, inTranslation, nullptr, nullptr).GetUUID();
		}

		uint64_t Scene_InstantiateChildPrefabWithTransform(uint64_t parentID, AssetHandle* prefabHandle, glm::vec3* inTranslation, glm::vec3* inRotation, glm::vec3* inScale)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity parent = scene->TryGetEntityWithUUID(parentID);
			BEY_ICALL_VALIDATE_PARAM_V(parent, parentID);
			BEY_ICALL_VALIDATE_PARAM_V(prefabHandle, "nullptr");

			Ref<Prefab> prefab = AssetManager::GetAsset<Prefab>(*prefabHandle);
			if (prefab == nullptr)
			{
				ErrorWithTrace("Cannot instantiate prefab. No prefab with handle {} found.", *prefabHandle);
				return 0;
			}

			return scene->InstantiateChild(prefab, parent, inTranslation, inRotation, inScale).GetUUID();
		}

		void Scene_DestroyEntity(uint64_t entityID)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			scene->SubmitToDestroyEntity(entity);
		}

		void Scene_DestroyAllChildren(uint64_t entityID)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const std::vector<UUID> children = entity.Children();
			for (UUID id : children)
				scene->DestroyEntity(id);
		}

		MonoArray* Scene_GetEntities()
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");

			auto entities = scene->GetAllEntitiesWith<IDComponent>();
			MonoArray* result = ManagedArrayUtils::Create<Entity>(entities.size());
			uint32_t i = 0;
			for (auto entity : entities)
				ManagedArrayUtils::SetValue(result, i++, entities.get<IDComponent>(entity).ID);

			return result;
		}

		MonoArray* Scene_GetChildrenIDs(uint64_t entityID)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const auto& children = entity.Children();
			MonoArray* result = ManagedArrayUtils::Create<uint64_t>(children.size());

			for (size_t i = 0; i < children.size(); i++)
				ManagedArrayUtils::SetValue(result, i, children[i]);

			return result;
		}

		void Scene_SetTimeScale(float timeScale)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene!");
			scene->SetTimeScale(timeScale);
		}

#pragma endregion

#pragma region Entity

		uint64_t Entity_GetParent(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			return entity.GetParentUUID();
		}

		void Entity_SetParent(uint64_t entityID, uint64_t parentID)
		{
			Entity child = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(child, entityID);

			if (parentID == 0)
			{
				ScriptEngine::GetSceneContext()->UnparentEntity(child);
			}
			else
			{
				Entity parent = GetEntity(parentID);
				BEY_ICALL_VALIDATE_PARAM_V(parent, parentID);
				child.SetParent(parent);
			}
		}

		MonoArray* Entity_GetChildren(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const auto& children = entity.Children();
			MonoArray* result = ManagedArrayUtils::Create<Entity>(children.size());
			for (uint32_t i = 0; i < children.size(); i++)
				ManagedArrayUtils::SetValue(result, i, children[i]);

			return result;
		}

		void Entity_CreateComponent(uint64_t entityID, MonoReflectionType* componentType)
		{
			if (componentType == nullptr)
			{
				ErrorWithTrace("Cannot add a null component to an entity.");
				return;
			}

			MonoType* managedComponentType = mono_reflection_type_get_type(componentType);
			char* componentTypeName = mono_type_get_name(managedComponentType);

			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (s_CreateComponentFuncs.find(managedComponentType) == s_CreateComponentFuncs.end())
			{
				ErrorWithTrace("Cannot add component of type '{}' to entity '{}'. That component hasn't been registered with the engine.", componentTypeName, entity.Name());
				mono_free(componentTypeName);
				return;
			}

			if (s_HasComponentFuncs.at(managedComponentType)(entity))
			{
				WarnWithTrace("Attempting to add duplicate component '{}' to entity '{}', ignoring.", componentTypeName, entity.Name());
				mono_free(componentTypeName);
				return;
			}

			s_CreateComponentFuncs.at(managedComponentType)(entity);
			mono_free(componentTypeName);
		}

		bool Entity_HasComponent(uint64_t entityID, MonoReflectionType* componentType)
		{
			BEY_PROFILE_FUNC();

			if (componentType == nullptr)
			{
				ErrorWithTrace("Attempting to check if entity has a component of a null type.");
				return false;
			}

			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (!entity)
				return false;

			MonoType* managedType = mono_reflection_type_get_type(componentType);

			if (s_HasComponentFuncs.find(managedType) == s_HasComponentFuncs.end())
			{
				char* componentTypeName = mono_type_get_name(managedType);
				ErrorWithTrace("Cannot check if entity '{}' has a component of type '{}'. That component hasn't been registered with the engine.", entity.Name(), componentTypeName);
				mono_free(componentTypeName);
				return false;
			}

			return s_HasComponentFuncs.at(managedType)(entity);
		}

		bool Entity_RemoveComponent(uint64_t entityID, MonoReflectionType* componentType)
		{
			BEY_PROFILE_FUNC();

			if (componentType == nullptr)
			{
				ErrorWithTrace("Attempting to remove a component of a null type from an entity.");
				return false;
			}

			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			MonoType* managedType = mono_reflection_type_get_type(componentType);
			char* componentTypeName = mono_type_get_name(managedType);
			if (s_RemoveComponentFuncs.find(managedType) == s_RemoveComponentFuncs.end())
			{
				ErrorWithTrace("Cannot remove a component of type '{}' from entity '{}'. That component hasn't been registered with the engine.", componentTypeName, entity.Name());
				return false;
			}

			if (!s_HasComponentFuncs.at(managedType)(entity))
			{
				WarnWithTrace("Tried to remove component '{}' from entity '{}' even though it doesn't have that component.", componentTypeName, entity.Name());
				return false;
			}

			mono_free(componentTypeName);
			s_RemoveComponentFuncs.at(managedType)(entity);
			return true;
		}

#pragma endregion

#pragma region TagComponent

		MonoString* TagComponent_GetTag(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const auto& tagComponent = entity.GetComponent<TagComponent>();
			return ScriptUtils::UTF8StringToMono(eastl::string(tagComponent.Tag.c_str(), tagComponent.Tag.size()));
		}

		void TagComponent_SetTag(uint64_t entityID, MonoString* inTag)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			auto& tagComponent = entity.GetComponent<TagComponent>();
			tagComponent.Tag = ScriptUtils::MonoStringToUTF8(inTag).c_str();
		}

#pragma endregion

		Ref<PhysicsBody> GetRigidBody(uint64_t entityID)
		{
			Ref<Scene> entityScene = ScriptEngine::GetSceneContext();

			if (!entityScene->IsPlaying())
				return nullptr;

			Entity entity = entityScene->TryGetEntityWithUUID(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			Ref<PhysicsScene> physicsScene = entityScene->GetPhysicsScene();

			if (!physicsScene)
				return nullptr;

			return physicsScene->GetEntityBody(entity);
		}

		Ref<CharacterController> GetCharacterController(uint64_t entityID)
		{
			Ref<Scene> entityScene = ScriptEngine::GetSceneContext();

			if (!entityScene->IsPlaying())
				return nullptr;

			Entity entity = entityScene->TryGetEntityWithUUID(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			Ref<PhysicsScene> physicsScene = entityScene->GetPhysicsScene();

			if (!physicsScene)
				return nullptr;

			return physicsScene->GetCharacterController(entity);
		}

#pragma region TransformComponent

		void TransformComponent_GetTransform(uint64_t entityID, Transform* outTransform)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const auto& tc = entity.GetComponent<TransformComponent>();
			outTransform->Translation = tc.Translation;
			outTransform->Rotation = tc.GetRotationEuler();
			outTransform->Scale = tc.Scale;
		}

		void TransformComponent_SetTransform(uint64_t entityID, Transform* inTransform)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);


			if (inTransform == nullptr)
			{
				ErrorWithTrace("Attempting to set a null transform for entity '{}'", entity.Name());
				return;
			}

			auto& tc = entity.GetComponent<TransformComponent>();
			tc.Translation = inTransform->Translation;
			tc.SetRotationEuler(inTransform->Rotation);
			tc.Scale = inTransform->Scale;
		}

		void TransformComponent_GetTranslation(uint64_t entityID, glm::vec3* outTranslation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			*outTranslation = entity.GetComponent<TransformComponent>().Translation;
		}

		void TransformComponent_SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (inTranslation == nullptr)
			{
				ErrorWithTrace("Attempting to set null translation for entity '{}'", entity.Name());
				return;
			}

			if (entity.HasComponent<RigidBodyComponent>())
			{
				const auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

				if (rigidBodyComponent.BodyType != EBodyType::Static)
				{
					WarnWithTrace("Trying to set translation for non-static RigidBody. This isn't allowed, and would result in unstable physics behavior.");
					return;
				}

				GetRigidBody(entityID)->SetTranslation(*inTranslation);
			}
			else if (entity.HasComponent<CharacterControllerComponent>())
			{
				auto characterController = GetCharacterController(entity.GetUUID());

				if (!characterController)
				{
					ErrorWithTrace("No character controller found for entity '{}'!", entity.Name());
					return;
				}

				WarnWithTrace("Setting the position of a character controller, this could lead to unstable behavior. Prefer using the CharacterControllerComponent.Move method instead");
				characterController->SetTranslation(*inTranslation);
			}
			else
			{
				entity.GetComponent<TransformComponent>().Translation = *inTranslation;
			}
		}

		void TransformComponent_GetRotation(uint64_t entityID, glm::vec3* outRotation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			*outRotation = entity.GetComponent<TransformComponent>().GetRotationEuler();
		}

		void TransformComponent_SetRotation(uint64_t entityID, glm::vec3* inRotation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (inRotation == nullptr)
			{
				ErrorWithTrace("Attempting to set null rotation for entity '{}'!", entity.Name());
				return;
			}

			if (entity.HasComponent<RigidBodyComponent>())
			{
				const auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

				if (rigidBodyComponent.BodyType != EBodyType::Static)
				{
					WarnWithTrace("Trying to set translation for non-static RigidBody. This isn't allowed, and would result in unstable physics behavior.");
					return;
				}

				GetRigidBody(entityID)->SetRotation(glm::quat(*inRotation));
			}
			else if (entity.HasComponent<CharacterControllerComponent>())
			{
				auto characterController = GetCharacterController(entity.GetUUID());

				if (!characterController)
				{
					ErrorWithTrace("No character controller found for entity '{}'!", entity.Name());
					return;
				}

				characterController->SetRotation(glm::quat(*inRotation));
			}
			else
			{
				entity.GetComponent<TransformComponent>().SetRotationEuler(*inRotation);
			}
		}

		void TransformComponent_SetRotationQuat(uint64_t entityID, glm::quat* inRotation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (inRotation == nullptr)
			{
				ErrorWithTrace("Attempting to set null rotation for entity '{}'!", entity.Name());
				return;
			}

			if (entity.HasComponent<RigidBodyComponent>())
			{
				const auto& rigidBodyComponent = entity.GetComponent<RigidBodyComponent>();

				if (rigidBodyComponent.BodyType != EBodyType::Static)
				{
					WarnWithTrace("Trying to set rotation for non-static RigidBody. This isn't allowed, and would result in unstable physics behavior.");
					return;
				}

				GetRigidBody(entityID)->SetRotation(*inRotation);
			}
			else if (entity.HasComponent<CharacterControllerComponent>())
			{
				auto characterController = GetCharacterController(entity.GetUUID());

				if (!characterController)
				{
					ErrorWithTrace("No character controller found for entity '{}'!", entity.Name());
					return;
				}

				characterController->SetRotation(*inRotation);
			}
			else
			{
				entity.GetComponent<TransformComponent>().SetRotation(*inRotation);
			}
		}

		void TransformComponent_GetScale(uint64_t entityID, glm::vec3* outScale)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			*outScale = entity.GetComponent<TransformComponent>().Scale;
		}

		void TransformComponent_SetScale(uint64_t entityID, glm::vec3* inScale)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (!entity)
				return;

			if (inScale == nullptr)
			{
				ErrorWithTrace("Attempting to set null scale for entity '{}'!", entity.Name());
				return;
			}

			entity.GetComponent<TransformComponent>().Scale = *inScale;
		}

		void TransformComponent_GetWorldSpaceTransform(uint64_t entityID, Transform* outTransform)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();

			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			const auto& wt = scene->GetWorldSpaceTransform(entity);
			outTransform->Translation = wt.Translation;
			outTransform->Rotation = wt.GetRotationEuler();
			outTransform->Scale = wt.Scale;
		}

		void TransformComponent_GetTransformMatrix(uint64_t entityID, glm::mat4* outTransform)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			*outTransform = entity.Transform().GetTransform();
		}

		void TransformComponent_SetTransformMatrix(uint64_t entityID, glm::mat4* inTransform)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			entity.Transform().SetTransform(*inTransform);
		}

		void TransformMultiply_Native(Transform* inA, Transform* inB, Transform* outResult)
		{
			TransformComponent a;
			a.Translation = inA->Translation;
			a.SetRotationEuler(inA->Rotation);
			a.Scale = inA->Scale;

			TransformComponent b;
			b.Translation = inB->Translation;
			b.SetRotationEuler(inB->Rotation);
			b.Scale = inB->Scale;

			glm::mat4 transform = a.GetTransform() * b.GetTransform();
			b.SetTransform(transform);
			outResult->Translation = b.Translation;
			outResult->Rotation = b.GetRotationEuler();
			outResult->Scale = b.Scale;
		}

#pragma endregion

#pragma region MeshComponent

		bool MeshComponent_GetMesh(uint64_t entityID, AssetHandle* outHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

			if (entity.HasComponent<MeshComponent>())
			{
				const auto& meshComponent = entity.GetComponent<MeshComponent>();
				auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);

				if (!mesh)
				{
					ErrorWithTrace("Component has an invalid mesh asset!");
					*outHandle = AssetHandle(0);
					return false;
				}

				*outHandle = meshComponent.MeshAssetHandle;
				return true;

			}

			ErrorWithTrace("This message should never appear. If it does it means the engine is broken.");
			*outHandle = AssetHandle(0);
			return false;
		}

		void MeshComponent_SetMesh(uint64_t entityID, AssetHandle* meshHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshComponent>());
			auto& meshComponent = entity.GetComponent<MeshComponent>();
			meshComponent.MeshAssetHandle = *meshHandle;
		}

		bool MeshComponent_HasMaterial(uint64_t entityID, int32_t index)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshComponent>());
			const auto& meshComponent = entity.GetComponent<MeshComponent>();
			auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);
			Ref<MaterialTable> materialTable = meshComponent.MaterialTable;
			return (materialTable && materialTable->HasMaterial(index)) || mesh->GetMaterials()->HasMaterial(index);
		}

		bool MeshComponent_GetMaterial(uint64_t entityID, int index, AssetHandle* outHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshComponent>());

			const auto& meshComponent = entity.GetComponent<MeshComponent>();

			Ref<MaterialTable> materialTable = meshComponent.MaterialTable;

			if (materialTable->HasMaterial(index))
			{
				*outHandle = materialTable->GetMaterial(index);
			}
			else
			{
				auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);

				if (!mesh->GetMaterials()->HasMaterial(index))
				{
					*outHandle = AssetHandle(0);
					return false;
				}

				*outHandle = mesh->GetMaterials()->GetMaterial(index);
			}

			return true;
		}

		bool MeshComponent_GetIsRigged(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshComponent>());

			auto& meshComponent = entity.GetComponent<MeshComponent>();
			auto mesh = AssetManager::GetAsset<Mesh>(meshComponent.MeshAssetHandle);
			if (mesh)
			{
				auto meshSource = mesh->GetMeshSource();
				return meshSource ? meshSource->IsSubmeshRigged(meshComponent.SubmeshIndex) : false;
			}
			return false;
		}

#pragma endregion

#pragma region StaticMeshComponent

		bool StaticMeshComponent_GetMesh(uint64_t entityID, AssetHandle* outHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());

			const auto& meshComponent = entity.GetComponent<StaticMeshComponent>();
			auto mesh = AssetManager::GetAsset<StaticMesh>(meshComponent.StaticMeshAssetHandle);

			if (!mesh)
			{
				ErrorWithTrace("Component has an invalid mesh asset!");
				*outHandle = AssetHandle(0);
				return false;
			}

			*outHandle = meshComponent.StaticMeshAssetHandle;
			return true;
		}

		void StaticMeshComponent_SetMesh(uint64_t entityID, AssetHandle* meshHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());
			auto& meshComponent = entity.GetComponent<StaticMeshComponent>();
			meshComponent.StaticMeshAssetHandle = *meshHandle;
		}

		bool StaticMeshComponent_HasMaterial(uint64_t entityID, int index)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());
			const auto& meshComponent = entity.GetComponent<StaticMeshComponent>();
			auto mesh = AssetManager::GetAsset<StaticMesh>(meshComponent.StaticMeshAssetHandle);
			Ref<MaterialTable> materialTable = meshComponent.MaterialTable;
			return (materialTable && materialTable->HasMaterial(index)) || mesh->GetMaterials()->HasMaterial(index);
		}

		bool StaticMeshComponent_GetMaterial(uint64_t entityID, int index, AssetHandle* outHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());

			const auto& meshComponent = entity.GetComponent<StaticMeshComponent>();

			Ref<MaterialTable> materialTable = meshComponent.MaterialTable;

			if (materialTable->HasMaterial(index))
			{
				*outHandle = materialTable->GetMaterial(index);
			}
			else
			{
				auto mesh = AssetManager::GetAsset<StaticMesh>(meshComponent.StaticMeshAssetHandle);

				if (!mesh->GetMaterials()->HasMaterial(index))
				{
					*outHandle = AssetHandle(0);
					return false;
				}

				*outHandle = mesh->GetMaterials()->GetMaterial(index);
			}

			return true;
		}

		void StaticMeshComponent_SetMaterial(uint64_t entityID, int index, uint64_t materialHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());

			const auto& component = entity.GetComponent<StaticMeshComponent>();
			Ref<MaterialTable> materialTable = component.MaterialTable;

			if ((uint32_t)index >= materialTable->GetMaterialCount())
			{
				WarnWithTrace("Material index out of range: {0}. Expected index less than {1}", index, materialTable->GetMaterialCount());
				return;
			}

			materialTable->SetMaterial(index, materialHandle);
		}

		bool StaticMeshComponent_IsVisible(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());
			const auto& meshComponent = entity.GetComponent<StaticMeshComponent>();
			return meshComponent.Visible;
		}

		void StaticMeshComponent_SetVisible(uint64_t entityID, bool visible)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<StaticMeshComponent>());
			auto& meshComponent = entity.GetComponent<StaticMeshComponent>();
			meshComponent.Visible = visible;
		}

#pragma endregion

#pragma region AnimationComponent

		uint32_t Identifier_Get(MonoString* inName)
		{
			return Identifier(ScriptUtils::MonoStringToUTF8(inName));
		}


		bool AnimationComponent_GetInputBool(uint64_t entityID, uint32_t inputID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					return animationComponent.AnimationGraph->Ins.at(inputID).getBool();
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputBool() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputBool() - input with id {0} is not of boolean type!", inputID);
				}
			}
			return false;
		}

		void AnimationComponent_SetInputBool(uint64_t entityID, uint32_t inputID, bool value)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					animationComponent.AnimationGraph->Ins.at(inputID).set(value);
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputBool() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputBool() - input with id {0} is not of boolean type!", inputID);
				}
			}
		}


		int32_t AnimationComponent_GetInputInt(uint64_t entityID, uint32_t inputID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					return animationComponent.AnimationGraph->Ins.at(inputID).getInt32();
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputInt() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputInt() - input with id {0} is not of integer type!", inputID);
				}
			}
			return false;
		}

		void AnimationComponent_SetInputInt(uint64_t entityID, uint32_t inputID, int32_t value)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					animationComponent.AnimationGraph->Ins.at(inputID).set(value);
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputInt() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputInt() - input with id {0} is not of integer type!", inputID);
				}
			}
		}


		float AnimationComponent_GetInputFloat(uint64_t entityID, uint32_t inputID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					return animationComponent.AnimationGraph->Ins.at(inputID).getFloat32();
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputFloat() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputFloat() - input with id {0} is not of float type!", inputID);
				}
			}
			return false;
		}

		void AnimationComponent_SetInputFloat(uint64_t entityID, uint32_t inputID, float value)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					animationComponent.AnimationGraph->Ins.at(inputID).set(value);
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputFloat() - input with id {0} does not exist!", inputID);
				}
				catch (const choc::value::Error&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputFloat() - input with id {0} is not of float type!", inputID);
				}
			}
		}


		void AnimationComponent_GetInputVector3(uint64_t entityID, uint32_t inputID, glm::vec3* value)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					auto& input = animationComponent.AnimationGraph->Ins.at(inputID);
					if (input.isObjectWithClassName(type::type_name<glm::vec3>()))
					{
						memcpy(value, input.getRawData(), sizeof(glm::vec3));
					}
					else
					{
						BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputVector3() - input with id {0} is not of Vector3 type!", inputID);
					}
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.GetInputVector3() - input with id {0} does not exist!", inputID);
				}
			}
		}

		void AnimationComponent_SetInputVector3(uint64_t entityID, uint32_t inputId, const glm::vec3* value)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					auto& input = animationComponent.AnimationGraph->Ins.at(inputId);
					if (input.isObjectWithClassName(type::type_name<glm::vec3>()))
					{
						memcpy(input.getRawData(), value, sizeof(glm::vec3));
					}
					else
					{
						BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputVector3() - input with id {0} is not of Vector3 type!", inputId);
					}
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputVector3() - input with id {0} does not exist!", inputId);
				}
			}
		}


		void AnimationComponent_SetInputTrigger(uint64_t entityID, uint32_t inputID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());

			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				try
				{
					animationComponent.AnimationGraph->InEvs.at(inputID).Event(inputID);
				}
				catch (const std::out_of_range&)
				{
					BEY_CONSOLE_LOG_ERROR("AnimationComponent.SetInputTrigger() - input with id {0} does not exist!", inputID);
				}
			}
		}


		void AnimationComponent_GetRootMotion(uint64_t entityID, Transform* outTransform)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AnimationComponent>());
			auto& animationComponent = entity.GetComponent<AnimationComponent>();
			if (animationComponent.AnimationGraph)
			{
				// RootMotion is at [0]th index of the pose
				Pose* pose = reinterpret_cast<Pose*>(animationComponent.AnimationGraph->EndpointOutputStreams.InValue(AnimationGraph::IDs::Pose).getRawData());
				outTransform->Translation = pose->RootMotion.Translation;
				outTransform->Rotation = glm::eulerAngles(pose->RootMotion.Rotation);
				outTransform->Scale = pose->RootMotion.Scale;
			}
		}

#pragma endregion

#pragma region SpotLightComponent

		void SpotLightComponent_GetRadiance(uint64_t entityID, glm::vec3* outRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			*outRadiance = entity.GetComponent<SpotLightComponent>().Radiance;
		}

		void SpotLightComponent_SetRadiance(uint64_t entityID, glm::vec3* inRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			BEY_ICALL_VALIDATE_PARAM_V(inRadiance, "nullptr");
			entity.GetComponent<SpotLightComponent>().Radiance = *inRadiance;
		}

		float SpotLightComponent_GetIntensity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().Intensity;
		}

		void SpotLightComponent_SetIntensity(uint64_t entityID, float intensity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().Intensity = intensity;
		}

		float SpotLightComponent_GetRange(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().Range;
		}

		void SpotLightComponent_SetRange(uint64_t entityID, float range)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().Range = range;
		}

		float SpotLightComponent_GetAngle(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().Angle;
		}

		void SpotLightComponent_SetAngle(uint64_t entityID, float angle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().Angle = angle;
		}

		float SpotLightComponent_GetFalloff(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().Falloff;
		}

		void SpotLightComponent_SetFalloff(uint64_t entityID, float falloff)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().Falloff = falloff;
		}

		float SpotLightComponent_GetAngleAttenuation(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().AngleAttenuation;
		}

		void SpotLightComponent_SetAngleAttenuation(uint64_t entityID, float angleAttenuation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().AngleAttenuation = angleAttenuation;
		}

		bool SpotLightComponent_GetCastsShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().CastShadows;
		}

		void SpotLightComponent_SetCastsShadows(uint64_t entityID, bool castsShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().CastShadows = castsShadows;
		}

		bool SpotLightComponent_GetSoftShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			return entity.GetComponent<SpotLightComponent>().SoftShadows;
		}

		void SpotLightComponent_SetSoftShadows(uint64_t entityID, bool softShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpotLightComponent>());
			entity.GetComponent<SpotLightComponent>().SoftShadows = softShadows;
		}

#pragma endregion

#pragma region ScriptComponent

		MonoObject* ScriptComponent_GetInstance(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<ScriptComponent>());

			const auto& component = entity.GetComponent<ScriptComponent>();

			if (!ScriptEngine::IsModuleValid(component.ScriptClassHandle))
			{
				ErrorWithTrace("Entity is referencing an invalid C# class!");
				return nullptr;
			}

			if (!ScriptEngine::IsEntityInstantiated(entity))
			{
				// Check if the entity is instantiated WITHOUT checking if the OnCreate method has run
				if (ScriptEngine::IsEntityInstantiated(entity, false))
				{
					// If so, call OnCreate here...
					ScriptEngine::CallMethod(component.ManagedInstance, "OnCreate");

					// NOTE: Don't use scriptComponent as a reference and modify it here
					//				If OnCreate spawns a lot of entities we would loose our reference
					//				to the script component...
					entity.GetComponent<ScriptComponent>().IsRuntimeInitialized = true;

					return GCManager::GetReferencedObject(component.ManagedInstance);
				}
				else if (component.ManagedInstance == nullptr)
				{
					ScriptEngine::RuntimeInitializeScriptEntity(entity);
					return GCManager::GetReferencedObject(component.ManagedInstance);
				}

				ErrorWithTrace("Entity '{0}' isn't instantiated?", entity.Name());
				return nullptr;
			}

			return GCManager::GetReferencedObject(component.ManagedInstance);
		}

#pragma endregion

#pragma region CameraComponent

		void CameraComponent_SetPerspective(uint64_t entityID, float inVerticalFOV, float inNearClip, float inFarClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetPerspective(inVerticalFOV, inNearClip, inFarClip);
		}

		void CameraComponent_SetOrthographic(uint64_t entityID, float inSize, float inNearClip, float inFarClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetOrthographic(inSize, inNearClip, inFarClip);
		}

		float CameraComponent_GetVerticalFOV(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			const auto& component = entity.GetComponent<CameraComponent>();
			return component.Camera.GetDegPerspectiveVerticalFOV();
		}

		void CameraComponent_SetVerticalFOV(uint64_t entityID, float inVerticalFOV)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			auto& component = entity.GetComponent<CameraComponent>();
			return component.Camera.SetDegPerspectiveVerticalFOV(inVerticalFOV);
		}

		float CameraComponent_GetPerspectiveNearClip(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			return camera.GetPerspectiveNearClip();
		}

		void CameraComponent_SetPerspectiveNearClip(uint64_t entityID, float inNearClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetPerspectiveNearClip(inNearClip);
		}

		float CameraComponent_GetPerspectiveFarClip(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			return camera.GetPerspectiveFarClip();
		}

		void CameraComponent_SetPerspectiveFarClip(uint64_t entityID, float inFarClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetPerspectiveFarClip(inFarClip);
		}

		float CameraComponent_GetOrthographicSize(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			return camera.GetOrthographicSize();
		}

		void CameraComponent_SetOrthographicSize(uint64_t entityID, float inSize)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetOrthographicSize(inSize);
		}

		float CameraComponent_GetOrthographicNearClip(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			return camera.GetOrthographicNearClip();
		}

		void CameraComponent_SetOrthographicNearClip(uint64_t entityID, float inNearClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetOrthographicNearClip(inNearClip);
		}

		float CameraComponent_GetOrthographicFarClip(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			return camera.GetOrthographicFarClip();
		}

		void CameraComponent_SetOrthographicFarClip(uint64_t entityID, float inFarClip)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			SceneCamera& camera = entity.GetComponent<CameraComponent>().Camera;
			camera.SetOrthographicFarClip(inFarClip);
		}

		CameraComponent::Type CameraComponent_GetProjectionType(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			const auto& component = entity.GetComponent<CameraComponent>();
			return component.ProjectionType;
		}

		void CameraComponent_SetProjectionType(uint64_t entityID, CameraComponent::Type inType)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			auto& component = entity.GetComponent<CameraComponent>();
			component.ProjectionType = inType;
			component.Camera.SetProjectionType((SceneCamera::ProjectionType)inType);
		}

		bool CameraComponent_GetPrimary(uint64_t entityID)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			const auto& component = entity.GetComponent<CameraComponent>();
			return component.Primary;
		}

		void CameraComponent_SetPrimary(uint64_t entityID, bool inValue)
		{
			Entity entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CameraComponent>());
			auto& component = entity.GetComponent<CameraComponent>();
			component.Primary = inValue;
		}

#pragma endregion

#pragma region DirectionalLightComponent

		void DirectionalLightComponent_GetRadiance(uint64_t entityID, glm::vec3* outRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			*outRadiance = entity.GetComponent<DirectionalLightComponent>().Radiance;
		}

		void DirectionalLightComponent_SetRadiance(uint64_t entityID, glm::vec3* inRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			entity.GetComponent<DirectionalLightComponent>().Radiance = *inRadiance;
		}

		float DirectionalLightComponent_GetIntensity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			return entity.GetComponent<DirectionalLightComponent>().Intensity;
		}

		void DirectionalLightComponent_SetIntensity(uint64_t entityID, float intensity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			entity.GetComponent<DirectionalLightComponent>().Intensity = intensity;
		}

		bool DirectionalLightComponent_GetCastShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			return entity.GetComponent<DirectionalLightComponent>().CastShadows;
		}

		void DirectionalLightComponent_SetCastShadows(uint64_t entityID, bool castShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			entity.GetComponent<DirectionalLightComponent>().CastShadows = castShadows;
		}

		bool DirectionalLightComponent_GetSoftShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			return entity.GetComponent<DirectionalLightComponent>().SoftShadows;
		}

		void DirectionalLightComponent_SetSoftShadows(uint64_t entityID, bool softShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			entity.GetComponent<DirectionalLightComponent>().SoftShadows = softShadows;
		}

		float DirectionalLightComponent_GetLightSize(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			return entity.GetComponent<DirectionalLightComponent>().SourceSize;
		}

		void DirectionalLightComponent_SetLightSize(uint64_t entityID, float lightSize)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<DirectionalLightComponent>());
			entity.GetComponent<DirectionalLightComponent>().SourceSize = lightSize;
		}

#pragma endregion

#pragma region PointLightComponent

		void PointLightComponent_GetRadiance(uint64_t entityID, glm::vec3* outRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			*outRadiance = entity.GetComponent<PointLightComponent>().Radiance;
		}

		void PointLightComponent_SetRadiance(uint64_t entityID, glm::vec3* inRadiance)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			BEY_ICALL_VALIDATE_PARAM_V(inRadiance, "nullptr");
			entity.GetComponent<PointLightComponent>().Radiance = *inRadiance;
		}

		float PointLightComponent_GetIntensity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().Intensity;
		}

		void PointLightComponent_SetIntensity(uint64_t entityID, float intensity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().Intensity = intensity;
		}

		float PointLightComponent_GetRadius(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().Radius;
		}

		void PointLightComponent_SetRadius(uint64_t entityID, float radius)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().Radius = radius;
		}

		float PointLightComponent_GetFalloff(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().Falloff;
		}

		void PointLightComponent_SetFalloff(uint64_t entityID, float falloff)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().Falloff = falloff;
		}

		float PointLightComponent_GetSize(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().SourceSize;
		}

		void PointLightComponent_SetSize(uint64_t entityID, float size)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().SourceSize = size;
		}

		bool PointLightComponent_GetCastsShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().CastShadows;
		}

		void PointLightComponent_SetCastsShadows(uint64_t entityID, bool castsShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().CastShadows = castsShadows;
		}

		bool PointLightComponent_GetSoftShadows(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			return entity.GetComponent<PointLightComponent>().SoftShadows;
		}

		void PointLightComponent_SetSoftShadows(uint64_t entityID, bool softShadows)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<PointLightComponent>());
			entity.GetComponent<PointLightComponent>().SoftShadows = softShadows;
		}

#pragma endregion

#pragma region SkyLightComponent

		float SkyLightComponent_GetIntensity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			return entity.GetComponent<SkyLightComponent>().Intensity;
		}

		void SkyLightComponent_SetIntensity(uint64_t entityID, float intensity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			entity.GetComponent<SkyLightComponent>().Intensity = intensity;
		}

		float SkyLightComponent_GetTurbidity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			return entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.x;
		}

		void SkyLightComponent_SetTurbidity(uint64_t entityID, float turbidity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.x = turbidity;
		}

		float SkyLightComponent_GetAzimuth(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			return entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.y;
		}

		void SkyLightComponent_SetAzimuth(uint64_t entityID, float azimuth)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.y = azimuth;
		}

		float SkyLightComponent_GetInclination(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			return entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.z;
		}

		void SkyLightComponent_SetInclination(uint64_t entityID, float inclination)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SkyLightComponent>());
			entity.GetComponent<SkyLightComponent>().TurbidityAzimuthInclination.z = inclination;
		}

#pragma endregion

#pragma region SpriteRendererComponent

		void SpriteRendererComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			*outColor = entity.GetComponent<SpriteRendererComponent>().Color;
		}

		void SpriteRendererComponent_SetColor(uint64_t entityID, glm::vec4* inColor)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			entity.GetComponent<SpriteRendererComponent>().Color = *inColor;
		}

		float SpriteRendererComponent_GetTilingFactor(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			return entity.GetComponent<SpriteRendererComponent>().TilingFactor;
		}

		void SpriteRendererComponent_SetTilingFactor(uint64_t entityID, float tilingFactor)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			entity.GetComponent<SpriteRendererComponent>().TilingFactor = tilingFactor;
		}

		void SpriteRendererComponent_GetUVStart(uint64_t entityID, glm::vec2* outUVStart)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			*outUVStart = entity.GetComponent<SpriteRendererComponent>().UVStart;
		}

		void SpriteRendererComponent_SetUVStart(uint64_t entityID, glm::vec2* inUVStart)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			entity.GetComponent<SpriteRendererComponent>().UVStart = *inUVStart;
		}

		void SpriteRendererComponent_GetUVEnd(uint64_t entityID, glm::vec2* outUVEnd)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			*outUVEnd = entity.GetComponent<SpriteRendererComponent>().UVEnd;
		}

		void SpriteRendererComponent_SetUVEnd(uint64_t entityID, glm::vec2* inUVEnd)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SpriteRendererComponent>());
			entity.GetComponent<SpriteRendererComponent>().UVEnd = *inUVEnd;
		}

#pragma endregion


#pragma region RigidBodyComponent

		void RigidBodyComponent_AddForce(uint64_t entityID, glm::vec3* inForce, EForceMode forceMode)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);

			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->AddForce(*inForce, forceMode);
		}

		void RigidBodyComponent_AddForceAtLocation(uint64_t entityID, glm::vec3* inForce, glm::vec3* inLocation, EForceMode forceMode)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->AddForce(*inForce, *inLocation, forceMode);
		}

		void RigidBodyComponent_AddTorque(uint64_t entityID, glm::vec3* inTorque, EForceMode forceMode)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->AddTorque(*inTorque);
		}

		void RigidBodyComponent_GetLinearVelocity(uint64_t entityID, glm::vec3* outVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			*outVelocity = rigidBody->GetLinearVelocity();
		}

		void RigidBodyComponent_SetLinearVelocity(uint64_t entityID, glm::vec3* inVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			if (inVelocity == nullptr)
			{
				ErrorWithTrace("Cannot set linear velocity of RigidBody to null. Entity: '{}'", entity.Name());
				return;
			}

			rigidBody->SetLinearVelocity(*inVelocity);
		}

		void RigidBodyComponent_GetAngularVelocity(uint64_t entityID, glm::vec3* outVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			*outVelocity = rigidBody->GetAngularVelocity();
		}

		void RigidBodyComponent_SetAngularVelocity(uint64_t entityID, glm::vec3* inVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			if (inVelocity == nullptr)
			{
				ErrorWithTrace("Cannot set angular velocity to null for RigidBody '{}'", entity.Name());
				return;
			}

			rigidBody->SetAngularVelocity(*inVelocity);
		}

		float RigidBodyComponent_GetMaxLinearVelocity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return 0.0f;
			}

			return rigidBody->GetMaxLinearVelocity();
		}

		void RigidBodyComponent_SetMaxLinearVelocity(uint64_t entityID, float maxVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetMaxLinearVelocity(maxVelocity);
			rigidBody->GetEntity().GetComponent<RigidBodyComponent>().MaxLinearVelocity = maxVelocity;
		}

		float RigidBodyComponent_GetMaxAngularVelocity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return 0.0f;
			}

			return rigidBody->GetMaxAngularVelocity();
		}

		void RigidBodyComponent_SetMaxAngularVelocity(uint64_t entityID, float maxVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetMaxAngularVelocity(maxVelocity);
			rigidBody->GetEntity().GetComponent<RigidBodyComponent>().MaxAngularVelocity = maxVelocity;
		}

		float RigidBodyComponent_GetLinearDrag(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());
			return entity.GetComponent<RigidBodyComponent>().LinearDrag;
		}

		void RigidBodyComponent_SetLinearDrag(uint64_t entityID, float linearDrag)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetLinearDrag(linearDrag);

			entity.GetComponent<RigidBodyComponent>().LinearDrag = linearDrag;
		}

		float RigidBodyComponent_GetAngularDrag(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());
			return entity.GetComponent<RigidBodyComponent>().AngularDrag;
		}

		void RigidBodyComponent_SetAngularDrag(uint64_t entityID, float angularDrag)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetAngularDrag(angularDrag);

			entity.GetComponent<RigidBodyComponent>().AngularDrag = angularDrag;
		}

		void RigidBodyComponent_Rotate(uint64_t entityID, glm::vec3* inRotation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			auto rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Can't rotate RigidBody because entity {} doesn't have a RigidBody!", entity.Name());
				return;
			}

			if (inRotation == nullptr)
			{
				ErrorWithTrace("Cannot rotate by 'null' rotation!");
				return;
			}

			rigidBody->Rotate(*inRotation);
		}

		uint32_t RigidBodyComponent_GetLayer(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());
			return entity.GetComponent<RigidBodyComponent>().LayerID;
		}

		void RigidBodyComponent_SetLayer(uint64_t entityID, uint32_t layerID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			if (!PhysicsLayerManager::IsLayerValid(layerID))
			{
				ErrorWithTrace("Invalid layer ID '{}'!", layerID);
				return;
			}

			rigidBody->SetCollisionLayer(layerID);
			auto& component = entity.GetComponent<RigidBodyComponent>();
			component.LayerID = layerID;
		}

		MonoString* RigidBodyComponent_GetLayerName(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			const auto& component = entity.GetComponent<RigidBodyComponent>();

			if (!PhysicsLayerManager::IsLayerValid(component.LayerID))
			{
				ErrorWithTrace("Can't find a layer with ID '{0}'!", component.LayerID);
				return nullptr;
			}

			const auto& layer = PhysicsLayerManager::GetLayer(component.LayerID);
			return ScriptUtils::UTF8StringToMono(layer.Name);
		}

		void RigidBodyComponent_SetLayerByName(uint64_t entityID, MonoString* inName)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);

			if (rigidBody == nullptr)
			{
				ErrorWithTrace("Failed to find physics body for entity {}", entity.Name());
				return;
			}

			if (inName == nullptr)
			{
				ErrorWithTrace("Name is null!");
				return;
			}

			eastl::string layerName = ScriptUtils::MonoStringToUTF8(inName);

			if (!PhysicsLayerManager::IsLayerValid(layerName))
			{
				ErrorWithTrace("Invalid layer name '{0}'!", layerName);
				return;
			}

			const auto& layer = PhysicsLayerManager::GetLayer(layerName);

			rigidBody->SetCollisionLayer(layer.LayerID);

			auto& component = entity.GetComponent<RigidBodyComponent>();
			component.LayerID = layer.LayerID;
		}

		float RigidBodyComponent_GetMass(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return 0.0f;
			}

			return rigidBody->GetMass();
		}

		void RigidBodyComponent_SetMass(uint64_t entityID, float mass)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetMass(mass);
		}

		EBodyType RigidBodyComponent_GetBodyType(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());
			return entity.GetComponent<RigidBodyComponent>().BodyType;
		}

		void RigidBodyComponent_SetBodyType(uint64_t entityID, EBodyType type)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			auto& rigidbodyComponent = entity.GetComponent<RigidBodyComponent>();

			// Don't bother doing anything if the type hasn't changed. It can be very expensive to recreate bodies.
			if (rigidbodyComponent.BodyType == type)
				return;

			if (!rigidbodyComponent.EnableDynamicTypeChange)
			{
				ErrorWithTrace("Cannot change the body type of a RigidBody during runtime. Please check \"Enable Dynamic Type Change\" in the component properties for entity {}", entity.Name());
				return;
			}

			rigidbodyComponent.BodyType = type;

			auto physicsScene = Scene::GetScene(entity.GetSceneUUID())->GetPhysicsScene();
			physicsScene->SetBodyType(entity, type);
		}

		bool RigidBodyComponent_IsTrigger(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return false;
			}

			return rigidBody->IsTrigger();
		}

		void RigidBodyComponent_SetTrigger(uint64_t entityID, bool isTrigger)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->ChangeTriggerState(isTrigger);
		}

		void RigidBodyComponent_MoveKinematic(uint64_t entityID, glm::vec3* inTargetPosition, glm::vec3* inTargetRotation, float inDeltaSeconds)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			if (inTargetPosition == nullptr || inTargetRotation == nullptr)
			{
				ErrorWithTrace("targetPosition or targetRotation is null!");
				return;
			}

			rigidBody->MoveKinematic(*inTargetPosition, glm::quat(*inTargetRotation), inDeltaSeconds);
		}

		void RigidBodyComponent_SetAxisLock(uint64_t entityID, EActorAxis axis, bool value, bool forceWake)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetAxisLock(axis, value, forceWake);
		}

		bool RigidBodyComponent_IsAxisLocked(uint64_t entityID, EActorAxis axis)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return false;
			}

			return rigidBody->IsAxisLocked(axis);
		}

		uint32_t RigidBodyComponent_GetLockedAxes(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return 0;
			}

			return static_cast<uint32_t>(rigidBody->GetLockedAxes());
		}

		bool RigidBodyComponent_IsSleeping(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return false;
			}

			return rigidBody->IsSleeping();
		}

		void RigidBodyComponent_SetIsSleeping(uint64_t entityID, bool isSleeping)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->SetSleepState(isSleeping);
		}

		void RigidBodyComponent_AddRadialImpulse(uint64_t entityID, glm::vec3* inOrigin, float radius, float strength, EFalloffMode falloff, bool velocityChange)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			if (!rigidBody)
			{
				ErrorWithTrace("Couldn't find RigidBody for entity '{}'", entity.Name());
				return;
			}

			rigidBody->AddRadialImpulse(*inOrigin, radius, strength, falloff, velocityChange);
		}

		void RigidBodyComponent_Teleport(uint64_t entityID, glm::vec3* inTargetPosition, glm::vec3* inTargetRotation, bool inForce)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<RigidBodyComponent>());
			GetPhysicsScene()->Teleport(entity, *inTargetPosition, glm::quat(*inTargetRotation), inForce);
		}

#pragma endregion

#pragma region CharacterControllerComponent

		static inline Ref<CharacterController> GetPhysicsController(Entity entity)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No scene active!");
			Ref<PhysicsScene> physicsScene = scene->GetPhysicsScene();
			BEY_CORE_ASSERT(physicsScene, "No physics scene active!");
			return physicsScene->GetCharacterController(entity);
		}

		bool CharacterControllerComponent_GetIsGravityEnabled(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return false;
			}

			return controller->IsGravityEnabled();
		}

		void CharacterControllerComponent_SetIsGravityEnabled(uint64_t entityID, bool enabled)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			controller->SetGravityEnabled(enabled);
		}

		float CharacterControllerComponent_GetSlopeLimit(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());
			return entity.GetComponent<CharacterControllerComponent>().SlopeLimitDeg;
		}

		void CharacterControllerComponent_SetSlopeLimit(uint64_t entityID, float slopeLimit)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			auto slopeLimitClamped = glm::clamp(slopeLimit, 0.0f, 90.0f);
			entity.GetComponent<CharacterControllerComponent>().SlopeLimitDeg = slopeLimitClamped;
			controller->SetSlopeLimit(slopeLimitClamped);
		}

		float CharacterControllerComponent_GetStepOffset(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());
			return entity.GetComponent<CharacterControllerComponent>().StepOffset;
		}

		void CharacterControllerComponent_SetStepOffset(uint64_t entityID, float stepOffset)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			entity.GetComponent<CharacterControllerComponent>().StepOffset = stepOffset;
			controller->SetStepOffset(stepOffset);
		}

		void CharacterControllerComponent_SetTranslation(uint64_t entityID, glm::vec3* inTranslation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			if (inTranslation == nullptr)
			{
				ErrorWithTrace("Cannot set CharacterControllerComponent translation to a null vector!");
				return;
			}

			controller->SetTranslation(*inTranslation);
		}

		void CharacterControllerComponent_SetRotation(uint64_t entityID, glm::quat* inRotation)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			if (inRotation == nullptr)
			{
				ErrorWithTrace("Cannot set CharacterControllerComponent rotation to a null quaternion!");
				return;
			}

			controller->SetRotation(*inRotation);
		}

		void CharacterControllerComponent_Move(uint64_t entityID, glm::vec3* inDisplacement)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			if (inDisplacement == nullptr)
			{
				ErrorWithTrace("Cannot move CharacterControllerComponent by a null displacement!");
				return;
			}

			controller->Move(*inDisplacement);
		}

		void CharacterControllerComponent_Jump(uint64_t entityID, float jumpPower)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			controller->Jump(jumpPower);
		}

		void CharacterControllerComponent_GetLinearVelocity(uint64_t entityID, glm::vec3* outVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			*outVelocity = controller->GetLinearVelocity();
		}

		void CharacterControllerComponent_SetLinearVelocity(uint64_t entityID, const glm::vec3& inVelocity)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return;
			}

			controller->SetLinearVelocity(inVelocity);
		}

		bool CharacterControllerComponent_IsGrounded(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return false;
			}

			return controller->IsGrounded();
		}

		ECollisionFlags CharacterControllerComponent_GetCollisionFlags(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CharacterControllerComponent>());

			auto controller = GetPhysicsController(entity);
			if (!controller)
			{
				ErrorWithTrace("Could not find CharacterController for entity '{}'!", entity.Name());
				return ECollisionFlags::None;
			}

			return controller->GetCollisionFlags();
		}

#pragma endregion

#pragma region FixedJointComponent

		/*static inline Ref<JointBase> GetJoint(Entity entity)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No scene active!");
			Ref<PhysicsScene> physicsScene = scene->GetPhysicsScene();
			BEY_CORE_ASSERT(physicsScene, "No physics scene active!");
			return physicsScene->GetJoint(entity);
		}*/

		uint64_t FixedJointComponent_GetConnectedEntity(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.GetConnectedEntity - Invalid entity!");
				return 0;
			}

			return entity.GetComponent<FixedJointComponent>().ConnectedEntity;
		}

		void FixedJointComponent_SetConnectedEntity(uint64_t entityID, uint64_t connectedEntityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetConnectedEntity - Invalid entity!");
				return;
			}

			auto connectedEntity = GetEntity(connectedEntityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetConnectedEntity - Invalid connectedEntity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetConnectedEntity - No Joint found!");
				return;
			}

			joint->SetConnectedEntity(connectedEntity);*/
		}

		bool FixedJointComponent_IsBreakable(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.IsBreakable - Invalid entity!");
				return false;
			}

			return entity.GetComponent<FixedJointComponent>().IsBreakable;
		}

		void FixedJointComponent_SetIsBreakable(uint64_t entityID, bool isBreakable)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetIsBreakable - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetIsBreakable - No Joint found!");
				return;
			}

			const auto& component = entity.GetComponent<FixedJointComponent>();

			if (isBreakable)
				joint->SetBreakForceAndTorque(component.BreakForce, component.BreakTorque);
			else
				joint->SetBreakForceAndTorque(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());*/
		}

		bool FixedJointComponent_IsBroken(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.IsBroken - Invalid entity!");
				return false;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.IsBroken - No Joint found!");
				return false;
			}

			return joint->IsBroken();*/
			return false;
		}

		void FixedJointComponent_Break(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.Break - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.Break - No Joint found!");
				return;
			}

			joint->Break();*/
		}

		float FixedJointComponent_GetBreakForce(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.GetBreakForce - Invalid entity!");
				return 0.0f;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.GetBreakForce - No Joint found!");
				return 0.0f;
			}

			float breakForce, breakTorque;
			joint->GetBreakForceAndTorque(breakForce, breakTorque);
			return breakForce;*/
			return 0.0f;
		}

		void FixedJointComponent_SetBreakForce(uint64_t entityID, float breakForce)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetBreakForce - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetBreakForce - No Joint found!");
				return;
			}

			float prevBreakForce, breakTorque;
			joint->GetBreakForceAndTorque(prevBreakForce, breakTorque);
			joint->SetBreakForceAndTorque(breakForce, breakTorque);*/
		}

		float FixedJointComponent_GetBreakTorque(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.GetBreakTorque - Invalid entity!");
				return 0.0f;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.GetBreakTorque - No Joint found!");
				return 0.0f;
			}

			float breakForce, breakTorque;
			joint->GetBreakForceAndTorque(breakForce, breakTorque);
			return breakTorque;*/
			return 0.0f;
		}

		void FixedJointComponent_SetBreakTorque(uint64_t entityID, float breakTorque)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetBreakTorque - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetBreakTorque - No Joint found!");
				return;
			}

			float breakForce, prevBreakTorque;
			joint->GetBreakForceAndTorque(breakForce, prevBreakTorque);
			joint->SetBreakForceAndTorque(breakForce, breakTorque);*/
		}

		bool FixedJointComponent_IsCollisionEnabled(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.IsCollisionEnabled - Invalid entity!");
				return false;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.IsCollisionEnabled - No Joint found!");
				return false;
			}

			return joint->IsCollisionEnabled();*/
			return false;
		}

		void FixedJointComponent_SetCollisionEnabled(uint64_t entityID, bool isCollisionEnabled)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetCollisionEnabled - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetCollisionEnabled - No Joint found!");
				return;
			}

			joint->SetCollisionEnabled(isCollisionEnabled);*/
		}

		bool FixedJointComponent_IsPreProcessingEnabled(uint64_t entityID)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.IsPreProcessingEnabled - Invalid entity!");
				return false;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.IsPreProcessingEnabled - No Joint found!");
				return false;
			}

			return joint->IsPreProcessingEnabled();*/
			return false;
		}

		void FixedJointComponent_SetPreProcessingEnabled(uint64_t entityID, bool isPreProcessingEnabled)
		{
			/*auto entity = GetEntity(entityID);
			if (!entity)
			{
				ErrorWithTrace("FixedJointComponent.SetPreProcessingEnabled - Invalid entity!");
				return;
			}

			auto joint = GetJoint(entity);
			if (!joint)
			{
				ErrorWithTrace("FixedJointComponent.SetPreProcessingEnabled - No Joint found!");
				return;
			}

			joint->SetPreProcessingEnabled(isPreProcessingEnabled);*/
		}

#pragma endregion

#pragma region BoxColliderComponent

		void BoxColliderComponent_GetHalfSize(uint64_t entityID, glm::vec3* outSize)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<BoxColliderComponent>());

			*outSize = entity.GetComponent<BoxColliderComponent>().HalfSize;
		}

		void BoxColliderComponent_GetOffset(uint64_t entityID, glm::vec3* outOffset)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<BoxColliderComponent>());

			*outOffset = entity.GetComponent<BoxColliderComponent>().Offset;
		}

		bool BoxColliderComponent_GetMaterial(uint64_t entityID, ColliderMaterial* outMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<BoxColliderComponent>());

			*outMaterial = entity.GetComponent<BoxColliderComponent>().Material;
			return true;
		}

		void BoxColliderComponent_SetMaterial(uint64_t entityID, ColliderMaterial* inMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<BoxColliderComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			BEY_CORE_VERIFY(rigidBody);

			for (uint32_t i = 0; i < rigidBody->GetShapeCount(ShapeType::Box); i++)
				rigidBody->GetShape(ShapeType::Box, i)->SetMaterial(*inMaterial);

			entity.GetComponent<BoxColliderComponent>().Material = *inMaterial;
		}

#pragma endregion

#pragma region SphereColliderComponent

		float SphereColliderComponent_GetRadius(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SphereColliderComponent>());
			return entity.GetComponent<SphereColliderComponent>().Radius;
		}

		void SphereColliderComponent_GetOffset(uint64_t entityID, glm::vec3* outOffset)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SphereColliderComponent>());

			*outOffset = entity.GetComponent<SphereColliderComponent>().Offset;
		}

		bool SphereColliderComponent_GetMaterial(uint64_t entityID, ColliderMaterial* outMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SphereColliderComponent>());

			*outMaterial = entity.GetComponent<SphereColliderComponent>().Material;
			return true;
		}

		void SphereColliderComponent_SetMaterial(uint64_t entityID, ColliderMaterial* inMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<SphereColliderComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			BEY_CORE_VERIFY(rigidBody);

			for (uint32_t i = 0; i < rigidBody->GetShapeCount(ShapeType::Sphere); i++)
				rigidBody->GetShape(ShapeType::Sphere, i)->SetMaterial(*inMaterial);

			entity.GetComponent<SphereColliderComponent>().Material = *inMaterial;
		}

#pragma endregion

#pragma region CapsuleColliderComponent

		float CapsuleColliderComponent_GetRadius(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CapsuleColliderComponent>());

			return entity.GetComponent<CapsuleColliderComponent>().Radius;
		}

		float CapsuleColliderComponent_GetHeight(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CapsuleColliderComponent>());

			return entity.GetComponent<CapsuleColliderComponent>().HalfHeight;
		}

		void CapsuleColliderComponent_GetOffset(uint64_t entityID, glm::vec3* outOffset)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CapsuleColliderComponent>());

			*outOffset = entity.GetComponent<CapsuleColliderComponent>().Offset;
		}

		bool CapsuleColliderComponent_GetMaterial(uint64_t entityID, ColliderMaterial* outMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CapsuleColliderComponent>());

			*outMaterial = entity.GetComponent<CapsuleColliderComponent>().Material;
			return true;
		}

		void CapsuleColliderComponent_SetMaterial(uint64_t entityID, ColliderMaterial* inMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<CapsuleColliderComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			BEY_CORE_VERIFY(rigidBody);

			for (uint32_t i = 0; i < rigidBody->GetShapeCount(ShapeType::Capsule); i++)
				rigidBody->GetShape(ShapeType::Capsule, i)->SetMaterial(*inMaterial);

			entity.GetComponent<CapsuleColliderComponent>().Material = *inMaterial;
		}

#pragma endregion

#pragma region MeshColliderComponent

		bool MeshColliderComponent_IsMeshStatic(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshColliderComponent>());

			const auto& component = entity.GetComponent<MeshColliderComponent>();
			Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);

			if (!colliderAsset)
			{
				ErrorWithTrace("Invalid collider asset!");
				return false;
			}

			if (!AssetManager::IsAssetHandleValid(colliderAsset->ColliderMesh))
			{
				ErrorWithTrace("Invalid mesh!");
				return false;
			}

			return AssetManager::GetAssetType(colliderAsset->ColliderMesh) == AssetType::StaticMesh;
		}

		bool MeshColliderComponent_IsColliderMeshValid(uint64_t entityID, AssetHandle* meshHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshColliderComponent>());

			const auto& component = entity.GetComponent<MeshColliderComponent>();
			Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);

			if (!colliderAsset)
			{
				ErrorWithTrace("Invalid collider asset!");
				return false;
			}

			return *meshHandle == colliderAsset->ColliderMesh;
		}

		bool MeshColliderComponent_GetColliderMesh(uint64_t entityID, AssetHandle* outHandle)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshColliderComponent>());

			const auto& component = entity.GetComponent<MeshColliderComponent>();
			Ref<MeshColliderAsset> colliderAsset = AssetManager::GetAsset<MeshColliderAsset>(component.ColliderAsset);

			if (!colliderAsset)
			{
				ErrorWithTrace("Invalid collider asset!");
				return false;
			}

			if (!AssetManager::IsAssetHandleValid(colliderAsset->ColliderMesh))
			{
				ErrorWithTrace("This component doesn't have a valid collider mesh!");
				*outHandle = AssetHandle(0);
				return false;
			}

			*outHandle = colliderAsset->ColliderMesh;
			return true;
		}

		bool MeshColliderComponent_GetMaterial(uint64_t entityID, ColliderMaterial* outMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshColliderComponent>());

			*outMaterial = entity.GetComponent<MeshColliderComponent>().Material;
			return true;
		}

		void MeshColliderComponent_SetMaterial(uint64_t entityID, ColliderMaterial* inMaterial)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<MeshColliderComponent>());

			Ref<PhysicsBody> rigidBody = GetRigidBody(entityID);
			BEY_CORE_VERIFY(rigidBody);

			for (uint32_t i = 0; i < rigidBody->GetShapeCount(ShapeType::ConvexMesh); i++)
				rigidBody->GetShape(ShapeType::ConvexMesh, i)->SetMaterial(*inMaterial);

			for (uint32_t i = 0; i < rigidBody->GetShapeCount(ShapeType::TriangleMesh); i++)
				rigidBody->GetShape(ShapeType::TriangleMesh, i)->SetMaterial(*inMaterial);

			entity.GetComponent<MeshColliderComponent>().Material = *inMaterial;
		}

#pragma endregion

#pragma region MeshCollider

		bool MeshCollider_IsStaticMesh(AssetHandle* meshHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
				return false;

			if (AssetManager::GetAssetType(*meshHandle) != AssetType::StaticMesh && AssetManager::GetAssetType(*meshHandle) != AssetType::Mesh)
			{
				WarnWithTrace("MeshCollider recieved AssetHandle to a non-mesh asset?");
				return false;
			}

			return AssetManager::GetAssetType(*meshHandle) == AssetType::StaticMesh;
		}

#pragma endregion

#pragma region AudioComponent

		bool AudioComponent_IsPlaying(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return AudioPlayback::IsPlaying(entityID);
		}

		bool AudioComponent_Play(uint64_t entityID, float startTime)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return AudioPlayback::Play(entityID, startTime);
		}

		bool AudioComponent_Stop(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return AudioPlayback::StopActiveSound(entityID);
		}

		bool AudioComponent_Pause(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return AudioPlayback::PauseActiveSound(entityID);
		}

		bool AudioComponent_Resume(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return AudioPlayback::Resume(entityID);
		}

		float AudioComponent_GetVolumeMult(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return entity.GetComponent<AudioComponent>().VolumeMultiplier;
		}

		void AudioComponent_SetVolumeMult(uint64_t entityID, float volumeMultiplier)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			entity.GetComponent<AudioComponent>().VolumeMultiplier = volumeMultiplier;
		}

		float AudioComponent_GetPitchMult(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			return entity.GetComponent<AudioComponent>().PitchMultiplier;
		}

		void AudioComponent_SetPitchMult(uint64_t entityID, float pitchMultiplier)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			entity.GetComponent<AudioComponent>().PitchMultiplier = pitchMultiplier;
		}

		void AudioComponent_SetEvent(uint64_t entityID, Audio::CommandID eventID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<AudioComponent>());

			if (!AudioCommandRegistry::DoesCommandExist<Audio::TriggerCommand>(eventID))
			{
				ErrorWithTrace("TriggerCommand with ID {0} does not exist!", eventID);
				return;
			}

			auto& component = entity.GetComponent<AudioComponent>();
			component.StartCommandID = eventID;
			component.StartEvent = AudioCommandRegistry::GetCommand<Audio::TriggerCommand>(eventID).DebugName;
		}

#pragma endregion

#pragma region TextComponent

		size_t TextComponent_GetHash(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<TextComponent>());

			return entity.GetComponent<TextComponent>().TextHash;
		}

		MonoString* TextComponent_GetText(uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<TextComponent>());

			const auto& component = entity.GetComponent<TextComponent>();
			return ScriptUtils::UTF8StringToMono(eastl::string(component.TextString.c_str(), component.TextString.size()));
		}

		void TextComponent_SetText(uint64_t entityID, MonoString* text)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<TextComponent>());

			auto& component = entity.GetComponent<TextComponent>();
			component.TextString = ScriptUtils::MonoStringToUTF8(text).c_str();
			component.TextHash = std::hash<std::string>()(component.TextString);
		}

		void TextComponent_GetColor(uint64_t entityID, glm::vec4* outColor)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<TextComponent>());

			const auto& component = entity.GetComponent<TextComponent>();
			*outColor = component.Color;
		}

		void TextComponent_SetColor(uint64_t entityID, glm::vec4* inColor)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);
			BEY_ICALL_VALIDATE_PARAM(entity.HasComponent<TextComponent>());

			auto& component = entity.GetComponent<TextComponent>();
			component.Color = *inColor;
		}

#pragma endregion

#pragma region Audio

		uint32_t Audio_PostEvent(Audio::CommandID eventID, uint64_t entityID)
		{
			if (!AudioCommandRegistry::DoesCommandExist<Audio::TriggerCommand>(eventID))
				return 0;

			return AudioPlayback::PostTrigger(eventID, entityID);
		}

		uint32_t Audio_PostEventFromAC(Audio::CommandID eventID, uint64_t entityID)
		{
			auto entity = GetEntity(entityID);
			BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);


			if (!AudioCommandRegistry::DoesCommandExist<Audio::TriggerCommand>(eventID))
			{
				ErrorWithTrace("Unable to find TriggerCommand with ID {0}", eventID);
				return 0;
			}

			return AudioPlayback::PostTriggerFromAC(eventID, entityID);
		}

		uint32_t Audio_PostEventAtLocation(Audio::CommandID eventID, Transform* inLocation)
		{
			if (!AudioCommandRegistry::DoesCommandExist<Audio::TriggerCommand>(eventID))
			{
				ErrorWithTrace("Unable to find TriggerCommand with ID {0}", eventID);
				return 0;
			}

			if (inLocation == nullptr)
			{
				ErrorWithTrace("Cannot post audio event at a location of 'null'!");
				return 0;
			}

			return AudioPlayback::PostTriggerAtLocation(eventID, inLocation->Translation, inLocation->Rotation, inLocation->Scale);
		}

		bool Audio_StopEventID(uint32_t playingEventID)
		{
			return AudioPlayback::StopEventID(playingEventID);
		}

		bool Audio_PauseEventID(uint32_t playingEventID)
		{
			return AudioPlayback::PauseEventID(playingEventID);
		}

		bool Audio_ResumeEventID(uint32_t playingEventID)
		{
			return AudioPlayback::ResumeEventID(playingEventID);
		}

		uint64_t Audio_CreateAudioEntity(Audio::CommandID eventID, Transform* inLocation, float volume, float pitch)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_VERIFY(scene, "No active scene");
			Entity entity = scene->CreateEntity("AudioEntity");

			if (!AudioCommandRegistry::DoesCommandExist<Audio::TriggerCommand>(eventID))
			{
				ErrorWithTrace("Unable to find TriggerCommand with ID {0}", eventID);
				return 0;
			}

			auto& component = entity.AddComponent<AudioComponent>();
			component.StartCommandID = eventID;
			component.StartEvent = AudioCommandRegistry::GetCommand<Audio::TriggerCommand>(eventID).DebugName;
			component.VolumeMultiplier = volume;
			component.PitchMultiplier = pitch;

			return entity.GetUUID();
		}

#pragma endregion

#pragma region AudioCommandID

		uint32_t AudioCommandID_Constructor(MonoString* inCommandName)
		{
			eastl::string commandName = ScriptUtils::MonoStringToUTF8(inCommandName);
			return Audio::CommandID(commandName.c_str());
		}

#pragma endregion

#pragma region AudioParameters
		//============================================================================================
		/// Audio Parameters Interface
		void Audio_SetParameterFloat(Audio::CommandID parameterID, uint64_t objectID, float value)
		{
			return AudioPlayback::SetParameterFloat(parameterID, objectID, value);
		}

		void Audio_SetParameterInt(Audio::CommandID parameterID, uint64_t objectID, int value)
		{
			return AudioPlayback::SetParameterInt(parameterID, objectID, value);
		}

		void Audio_SetParameterBool(Audio::CommandID parameterID, uint64_t objectID, bool value)
		{
			return AudioPlayback::SetParameterBool(parameterID, objectID, value);

		}

		void Audio_SetParameterFloatForAC(Audio::CommandID parameterID, uint64_t entityID, float value)
		{
			return AudioPlayback::SetParameterFloatForAC(parameterID, entityID, value);
		}

		void Audio_SetParameterIntForAC(Audio::CommandID parameterID, uint64_t entityID, int value)
		{
			return AudioPlayback::SetParameterIntForAC(parameterID, entityID, value);
		}

		void Audio_SetParameterBoolForAC(Audio::CommandID parameterID, uint64_t entityID, bool value)
		{
			return AudioPlayback::SetParameterBoolForAC(parameterID, entityID, value);
		}

		void Audio_SetParameterFloatForEvent(Audio::CommandID parameterID, uint32_t eventID, float value)
		{
			AudioPlayback::SetParameterFloat(parameterID, eventID, value);
		}

		void Audio_SetParameterIntForEvent(Audio::CommandID parameterID, uint32_t eventID, int value)
		{
			AudioPlayback::SetParameterInt(parameterID, eventID, value);
		}

		void Audio_SetParameterBoolForEvent(Audio::CommandID parameterID, uint32_t eventID, bool value)
		{
			AudioPlayback::SetParameterBool(parameterID, eventID, value);
		}

		//============================================================================================
		void Audio_PreloadEventSources(Audio::CommandID eventID)
		{
			AudioPlayback::PreloadEventSources(eventID);
		}

		void Audio_UnloadEventSources(Audio::CommandID eventID)
		{
			AudioPlayback::UnloadEventSources(eventID);
		}

		void Audio_SetLowPassFilterValue(uint64_t objectID, float value)
		{
			AudioPlayback::SetLowPassFilterValueObj(objectID, value);
		}

		void Audio_SetHighPassFilterValue(uint64_t objectID, float value)
		{
			AudioPlayback::SetHighPassFilterValueObj(objectID, value);
		}

		void Audio_SetLowPassFilterValue_Event(Audio::CommandID eventID, float value)
		{
			AudioPlayback::SetLowPassFilterValue(eventID, value);
		}

		void Audio_SetHighPassFilterValue_Event(Audio::CommandID eventID, float value)
		{
			AudioPlayback::SetHighPassFilterValue(eventID, value);
		}

		void Audio_SetLowPassFilterValue_AC(uint64_t entityID, float value)
		{
			AudioPlayback::SetLowPassFilterValueAC(entityID, value);
		}

		void Audio_SetHighPassFilterValue_AC(uint64_t entityID, float value)
		{
			AudioPlayback::SetHighPassFilterValueAC(entityID, value);
		}

#pragma endregion

#pragma region Texture2D

		bool Texture2D_Create(uint32_t width, uint32_t height, TextureWrap wrapMode, TextureFilter filterMode, AssetHandle* outHandle)
		{
			TextureSpecification spec;
			spec.Width = width;
			spec.Height = height;
			spec.SamplerWrap = wrapMode;
			spec.SamplerFilter = filterMode;
			spec.CreateBindlessDescriptor = true;
			spec.Compress = true;

			auto result = Texture2D::Create(spec);
			*outHandle = AssetManager::AddMemoryOnlyAsset<Texture2D>(result);
			return true;
		}

		void Texture2D_GetSize(AssetHandle* inHandle, uint32_t* outWidth, uint32_t* outHeight)
		{
			Ref<Texture2D> instance = AssetManager::GetAsset<Texture2D>(*inHandle);
			if (!instance)
			{
				ErrorWithTrace("Tried to get texture size using an invalid handle!");
				return;
			}

			*outWidth = instance->GetWidth();
			*outHeight = instance->GetHeight();
		}

		void Texture2D_SetData(AssetHandle* inHandle, MonoArray* inData)
		{
			Ref<Texture2D> instance = AssetManager::GetAsset<Texture2D>(*inHandle);

			if (!instance)
			{
				ErrorWithTrace("Tried to set texture data in an invalid texture!");
				return;
			}

			uintptr_t count = mono_array_length(inData);
			uint32_t dataSize = (uint32_t)(count * sizeof(glm::vec4) / 4);

			instance->Lock();
			Buffer buffer = instance->GetWriteableBuffer();
			BEY_CORE_ASSERT(dataSize <= buffer.Size);
			// Convert RGBA32F color to RGBA8
			uint8_t* pixels = (uint8_t*)buffer.Data;
			uint32_t index = 0;
			for (uint32_t i = 0; i < instance->GetWidth() * instance->GetHeight(); i++)
			{
				glm::vec4& value = mono_array_get(inData, glm::vec4, i);
				*pixels++ = (uint32_t)(value.x * 255.0f);
				*pixels++ = (uint32_t)(value.y * 255.0f);
				*pixels++ = (uint32_t)(value.z * 255.0f);
				*pixels++ = (uint32_t)(value.w * 255.0f);
			}

			instance->Unlock();
		}

		// TODO: Uncomment when Beyond can actually read texture data from the CPU or when image data is persistently stored in RAM
		/*MonoArray* Texture2D_GetData(AssetHandle* inHandle)
		{
			Ref<Texture2D> instance = AssetManager::GetAsset<Texture2D>(*inHandle);

			if (!instance)
			{
				ErrorWithTrace("Tried to get texture data for an invalid texture!");
				return nullptr;
			}

			uint32_t width = instance->GetWidth();
			uint32_t height = instance->GetHeight();
			ManagedArray result = ManagedArray::Create<glm::vec4>(width * height);

			instance->Lock();
			Buffer buffer = instance->GetImage()->GetBuffer();
			uint8_t* pixels = (uint8_t*)buffer.Data;

			for (uint32_t i = 0; i < width * height; i++)
			{
				glm::vec4 value;
				value.r = (float)*pixels++ / 255.0f;
				value.g = (float)*pixels++ / 255.0f;
				value.b = (float)*pixels++ / 255.0f;
				value.a = (float)*pixels++ / 255.0f;

				result.Set(i, value);
			}

			instance->Unlock();
			return result;
		}*/

#pragma endregion

#pragma region Mesh

		bool Mesh_GetMaterialByIndex(AssetHandle* meshHandle, uint32_t index, AssetHandle* outHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
			{
				ErrorWithTrace("Invalid Mesh instance!");
				*outHandle = AssetHandle(0);
				return false;
			}

			Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(*meshHandle);
			if (!mesh || !mesh->IsValid())
			{
				ErrorWithTrace("Invalid Mesh instance!");
				*outHandle = AssetHandle(0);
				return false;
			}

			Ref<MaterialTable> materialTable = mesh->GetMaterials();
			if (materialTable == nullptr)
			{
				ErrorWithTrace("Mesh has no materials!");
				*outHandle = AssetHandle(0);
				return false;
			}

			if (materialTable->GetMaterialCount() == 0)
			{
				*outHandle = AssetHandle(0);
				return false;
			}

			if ((uint32_t)index >= materialTable->GetMaterialCount())
			{
				ErrorWithTrace("Material index out of range. Index: {0}, MaxIndex: {1}", index, materialTable->GetMaterialCount() - 1);
				*outHandle = AssetHandle(0);
				return false;
			}

			*outHandle = materialTable->GetMaterial(index);
			return true;
		}

		int Mesh_GetMaterialCount(AssetHandle* meshHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
			{
				ErrorWithTrace("called on an invalid Mesh instance!");
				return 0;
			}

			Ref<Mesh> mesh = AssetManager::GetAsset<Mesh>(*meshHandle);
			if (!mesh || !mesh->IsValid())
			{
				ErrorWithTrace("called on an invalid Mesh instance!");
				return 0;
			}

			Ref<MaterialTable> materialTable = mesh->GetMaterials();
			if (materialTable == nullptr)
				return 0;

			return materialTable->GetMaterialCount();
		}

#pragma endregion

#pragma region StaticMesh

		bool StaticMesh_GetMaterialByIndex(AssetHandle* meshHandle, uint32_t index, AssetHandle* outHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
			{
				ErrorWithTrace("called on an invalid Mesh instance!");
				*outHandle = AssetHandle(0);
				return false;
			}

			Ref<StaticMesh> mesh = AssetManager::GetAsset<StaticMesh>(*meshHandle);
			if (!mesh || !mesh->IsValid())
			{
				ErrorWithTrace("called on an invalid Mesh instance!");
				*outHandle = AssetHandle(0);
				return false;
			}

			Ref<MaterialTable> materialTable = mesh->GetMaterials();
			if (materialTable == nullptr)
			{
				ErrorWithTrace("Mesh has no materials!");
				*outHandle = AssetHandle(0);
				return false;
			}

			if (materialTable->GetMaterialCount() == 0)
			{
				*outHandle = AssetHandle(0);
				return false;
			}

			if ((uint32_t)index >= materialTable->GetMaterialCount())
			{
				ErrorWithTrace("Material index out of range. Index: {0}, MaxIndex: {1}", index, materialTable->GetMaterialCount() - 1);
				*outHandle = AssetHandle(0);
				return false;
			}

			*outHandle = materialTable->GetMaterial(index);
			return true;
		}

		int StaticMesh_GetMaterialCount(AssetHandle* meshHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
			{
				ErrorWithTrace("Invalid Mesh instance!");
				return 0;
			}

			Ref<StaticMesh> mesh = AssetManager::GetAsset<StaticMesh>(*meshHandle);
			if (!mesh || !mesh->IsValid())
			{
				ErrorWithTrace("Invalid Mesh instance!");
				return 0;
			}

			Ref<MaterialTable> materialTable = mesh->GetMaterials();
			if (materialTable == nullptr)
				return 0;

			return materialTable->GetMaterialCount();
		}

#pragma endregion

#pragma region Material

		static Ref<MaterialAsset> Material_GetMaterialAsset(const char* functionName, uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle)
		{
			if (!AssetManager::IsAssetHandleValid(*meshHandle))
			{
				// NOTE: This means the material is expected to be an actual asset, referenced directly
				Ref<MaterialAsset> material = AssetManager::GetAsset<MaterialAsset>(*materialHandle);

				if (!material)
				{
					ErrorWithTrace("Failed to get a material asset with handle {}, no such asset exists!", *materialHandle);
					return nullptr;
				}

				return material;
			}

			Ref<MaterialTable> materialTable = nullptr;
			if (entityID == 0)
			{

				if (AssetManager::GetAssetType(*meshHandle) == AssetType::Mesh)
				{
					auto mesh = AssetManager::GetAsset<Mesh>(*meshHandle);

					if (!mesh || !mesh->IsValid())
					{
						ErrorWithTrace("Invalid mesh instance!");
						return nullptr;
					}

					materialTable = mesh->GetMaterials();
				}
				else if (AssetManager::GetAssetType(*meshHandle) == AssetType::StaticMesh)
				{
					auto staticMesh = AssetManager::GetAsset<StaticMesh>(*meshHandle);

					if (!staticMesh || !staticMesh->IsValid())
					{
						ErrorWithTrace("Invalid mesh instance!");
						return nullptr;
					}

					materialTable = staticMesh->GetMaterials();
				}
				else
				{
					ErrorWithTrace("meshHandle doesn't correspond with a Mesh? AssetType: {1}", Beyond::Utils::AssetTypeToString(AssetManager::GetAssetType(*meshHandle)));
					return nullptr;
				}
			}
			else
			{
				// This material is expected to be on a component
				auto entity = GetEntity(entityID);
				BEY_ICALL_VALIDATE_PARAM_V(entity, entityID);

				if (entity.HasComponent<MeshComponent>())
				{
					materialTable = entity.GetComponent<MeshComponent>().MaterialTable;
				}
				else if (entity.HasComponent<StaticMeshComponent>())
				{
					materialTable = entity.GetComponent<StaticMeshComponent>().MaterialTable;
				}
				else
				{
					ErrorWithTrace("Invalid component!");
					return nullptr;
				}
			}

			if (materialTable == nullptr || materialTable->GetMaterialCount() == 0)
			{
				ErrorWithTrace("Mesh has no materials!");
				return nullptr;
			}

			Ref<MaterialAsset> materialInstance = nullptr;

			for (const auto& [materialIndex, material] : materialTable->GetMaterials())
			{
				if (material == *materialHandle)
				{
					materialInstance = AssetManager::GetAsset<MaterialAsset>(material);
					break;
				}
			}

			if (materialInstance == nullptr)
				ErrorWithTrace("This appears to be an invalid Material!");

			return materialInstance;
		}

		void Material_GetAlbedoColor(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, glm::vec4* outAlbedoColor)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.GetAlbedoColor", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
			{
				*outAlbedoColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
				return;
			}

			*outAlbedoColor = materialInstance->GetAlbedoColor();
		}

		void Material_SetAlbedoColor(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, glm::vec4* inAlbedoColor)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.SetAlbedoColor", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			materialInstance->SetAlbedoColor(*inAlbedoColor);
		}

		float Material_GetMetalness(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.GetMetalness", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return 0.0f;

			return materialInstance->GetMetalness();
		}

		void Material_SetMetalness(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, float inMetalness)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.SetMetalness", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			materialInstance->SetMetalness(inMetalness);
		}

		float Material_GetRoughness(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.GetRoughness", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return {};

			return materialInstance->GetRoughness();
		}

		void Material_SetRoughness(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, float inRoughness)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.SetRoughness", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			materialInstance->SetRoughness(inRoughness);
		}

		float Material_GetEmission(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.GetEmission", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return 0.0f;

			return materialInstance->GetEmission();
		}

		void Material_SetEmission(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, float inEmission)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.SetEmission", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			materialInstance->SetEmission(inEmission);
		}

		void Material_SetFloat(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, MonoString* inUniform, float value)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.Set", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			eastl::string uniformName = ScriptUtils::MonoStringToUTF8(inUniform);
			if (uniformName.empty())
			{
				WarnWithTrace("Cannot set uniform with empty name!");
				return;
			}

			materialInstance->GetMaterial()->Set(uniformName, value);
		}

		void Material_SetVector3(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, MonoString* inUniform, glm::vec3* inValue)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.Set", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			eastl::string uniformName = ScriptUtils::MonoStringToUTF8(inUniform);
			if (uniformName.empty())
			{
				WarnWithTrace("Cannot set uniform with empty name!");
				return;
			}

			materialInstance->GetMaterial()->Set(uniformName, *inValue);
		}

		void Material_SetVector4(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, MonoString* inUniform, glm::vec3* inValue)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.Set", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			eastl::string uniformName = ScriptUtils::MonoStringToUTF8(inUniform);
			if (uniformName.empty())
			{
				WarnWithTrace("Cannot set uniform with empty name!");
				return;
			}

			materialInstance->GetMaterial()->Set(uniformName, *inValue);
		}

		void Material_SetTexture(uint64_t entityID, AssetHandle* meshHandle, AssetHandle* materialHandle, MonoString* inUniform, AssetHandle* inTexture)
		{
			Ref<MaterialAsset> materialInstance = Material_GetMaterialAsset("Material.Set", entityID, meshHandle, materialHandle);

			if (materialInstance == nullptr)
				return;

			eastl::string uniformName = ScriptUtils::MonoStringToUTF8(inUniform);
			if (uniformName.empty())
			{
				WarnWithTrace("Cannot set uniform with empty name!");
				return;
			}

			Ref<Texture2D> texture = AssetManager::GetAsset<Texture2D>(*inTexture);

			if (!texture)
			{
				ErrorWithTrace("Tried to set an invalid texture instance");
				return;
			}

			materialInstance->GetMaterial()->Set(uniformName, texture);
		}

#pragma endregion

#pragma region MeshFactory

		void* MeshFactory_CreatePlane(float width, float height)
		{
			// TODO: MeshFactory::CreatePlane(width, height, subdivisions)!
			return nullptr;
		}

#pragma endregion

#pragma region Physics

		bool Physics_CastRay(RaycastData* inRaycastData, ScriptRaycastHit* outHit)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");

			if (scene->IsEditorScene())
			{
				BEY_THROW_INVALID_OPERATION("Physics.Raycast can only be called in Play mode!");
				return false;
			}

			//BEY_CORE_ASSERT(scene->GetPhysicsScene()->IsValid());

			RayCastInfo rayCastInfo;
			rayCastInfo.Origin = inRaycastData->Origin;
			rayCastInfo.Direction = inRaycastData->Direction;
			rayCastInfo.MaxDistance = inRaycastData->MaxDistance;

			if (inRaycastData->ExcludeEntities)
			{
				size_t excludeEntitiesCount = mono_array_length(inRaycastData->ExcludeEntities);

				// NOTE: Same as calling the constructor with excludeEntitiesCount as the only argument
				rayCastInfo.ExcludedEntities.rehash(excludeEntitiesCount);
				
				for (size_t i = 0; i < excludeEntitiesCount; i++)
				{
					uint64_t entityID = mono_array_get(inRaycastData->ExcludeEntities, uint64_t, i);
					rayCastInfo.ExcludedEntities.insert(entityID);
				}
			}

			SceneQueryHit tempHit;
			bool success = scene->GetPhysicsScene()->CastRay(&rayCastInfo, tempHit);

			if (success && inRaycastData->RequiredComponentTypes != nullptr)
			{
				Entity entity = scene->GetEntityWithUUID(tempHit.HitEntity);
				size_t requiredComponentsCount = mono_array_length(inRaycastData->RequiredComponentTypes);

				for (size_t i = 0; i < requiredComponentsCount; i++)
				{
					void* reflectionType = mono_array_get(inRaycastData->RequiredComponentTypes, void*, i);
					if (reflectionType == nullptr)
					{
						ErrorWithTrace("Physics.Raycast - Why did you feel the need to pass a \"null\" as a required component?");
						success = false;
						break;
					}

					MonoType* componentType = mono_reflection_type_get_type((MonoReflectionType*)reflectionType);

#ifdef BEY_DEBUG
					MonoClass* typeClass = mono_type_get_class(componentType);
					MonoClass* parentClass = mono_class_get_parent(typeClass);

					bool validComponentFilter = parentClass != nullptr;
					if (validComponentFilter)
					{
						const char* parentClassName = mono_class_get_name(parentClass);
						const char* parentNameSpace = mono_class_get_namespace(parentClass);
						validComponentFilter = strstr(parentClassName, "Component") != nullptr && strstr(parentNameSpace, "Beyond") != nullptr;
					}

					if (!validComponentFilter)
					{
						ErrorWithTrace("Physics.Raycast - {0} does not inherit from Beyond.Component!", mono_class_get_name(typeClass));
						success = false;
						break;
					}
#endif

					if (!s_HasComponentFuncs[componentType](entity))
					{
						success = false;
						break;
					}
				}
			}

			if (success)
			{
				outHit->HitEntity = tempHit.HitEntity;
				outHit->Position = tempHit.Position;
				outHit->Normal = tempHit.Normal;
				outHit->Distance = tempHit.Distance;

				if (tempHit.HitCollider)
				{
					MonoObject* shapeInstance = nullptr;
					glm::vec3 offset(0.0f);

					switch (tempHit.HitCollider->GetType())
					{
						case ShapeType::Box:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<BoxColliderComponent>();
							offset = colliderComp.Offset;

							glm::vec3 halfSize = tempHit.HitCollider.As<BoxShape>()->GetHalfSize();
							CSharpInstance boxInstance("Beyond.BoxShape");
							shapeInstance = boxInstance.Instantiate(halfSize);
							break;
						}
						case ShapeType::Sphere:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<SphereColliderComponent>();
							offset = colliderComp.Offset;

							float radius = tempHit.HitCollider.As<SphereShape>()->GetRadius();
							CSharpInstance sphereInstance("Beyond.SphereShape");
							shapeInstance = sphereInstance.Instantiate(radius);
							break;
						}
						case ShapeType::Capsule:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<CapsuleColliderComponent>();
							offset = colliderComp.Offset;

							Ref<CapsuleShape> capsuleShape = tempHit.HitCollider.As<CapsuleShape>();
							float height = capsuleShape->GetHeight();
							float radius = capsuleShape->GetRadius();
							CSharpInstance capsuleInstance("Beyond.CapsuleShape");
							shapeInstance = capsuleInstance.Instantiate(height, radius);
							break;
						}
						case ShapeType::ConvexMesh:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							Ref<ConvexMeshShape> convexMeshShape = tempHit.HitCollider.As<ConvexMeshShape>();
							CSharpInstance meshInstance;
							CSharpInstance convexMeshInstance("Beyond.ConvexMeshShape");
							shapeInstance = convexMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
						case ShapeType::TriangleMesh:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							CSharpInstance triangleMeshInstance("Beyond.TriangleMeshShape");
							shapeInstance = triangleMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
					}

					if (shapeInstance != nullptr)
					{
						CSharpInstance colliderInstance("Beyond.Collider");
						outHit->HitCollider = colliderInstance.Instantiate(tempHit.HitEntity, shapeInstance, offset);
					}
				}
			}
			else
			{
				*outHit = ScriptRaycastHit();
			}

			return success;
		}

		bool Physics_CastShape(ShapeQueryData* inShapeCastData, ScriptRaycastHit* outHit)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");

			if (scene->IsEditorScene())
			{
				BEY_THROW_INVALID_OPERATION("Physics.Raycast can only be called in Play mode!");
				return false;
			}

			CSharpInstanceInspector inspector(inShapeCastData->ShapeDataInstance);
			if (!inspector.InheritsFrom("Beyond.Shape"))
			{
				BEY_CORE_VERIFY(false);
				return false;
			}

			ShapeCastInfo* shapeCastInfo = nullptr;

			ShapeType shapeType = inspector.GetFieldValue<ShapeType>("ShapeType");

			switch (shapeType)
			{
				case ShapeType::Box:
				{
					BoxCastInfo* boxCastInfo = hnew BoxCastInfo();
					boxCastInfo->HalfExtent = inspector.GetFieldValue<glm::vec3>("HalfExtent");
					shapeCastInfo = boxCastInfo;
					break;
				}
				case ShapeType::Sphere:
				{
					SphereCastInfo* sphereCastInfo = hnew SphereCastInfo();
					sphereCastInfo->Radius = inspector.GetFieldValue<float>("Radius");
					shapeCastInfo = sphereCastInfo;
					break;
				}
				case ShapeType::Capsule:
				{
					CapsuleCastInfo* capsuleCastInfo = hnew CapsuleCastInfo();
					capsuleCastInfo->HalfHeight = inspector.GetFieldValue<float>("HalfHeight");
					capsuleCastInfo->Radius = inspector.GetFieldValue<float>("Radius");
					shapeCastInfo = capsuleCastInfo;
					break;
				}
				case ShapeType::ConvexMesh:
				case ShapeType::TriangleMesh:
				case ShapeType::CompoundShape:
				case ShapeType::MutableCompoundShape:
				{
					WarnWithTrace("Can't do a shape cast with Convex, Triangle or Compound shapes!");
					return false;
				}
			}

			if (shapeCastInfo == nullptr)
				return false;

			shapeCastInfo->Origin = inShapeCastData->Origin;
			shapeCastInfo->Direction = inShapeCastData->Direction;
			shapeCastInfo->MaxDistance = inShapeCastData->MaxDistance;

			if (inShapeCastData->ExcludeEntities)
			{
				size_t excludeEntitiesCount = mono_array_length(inShapeCastData->ExcludeEntities);
				
				// NOTE: Same as calling the constructor with excludeEntitiesCount as the only argument
				shapeCastInfo->ExcludedEntities.rehash(excludeEntitiesCount);

				for (size_t i = 0; i < excludeEntitiesCount; i++)
				{
					uint64_t entityID = mono_array_get(inShapeCastData->ExcludeEntities, uint64_t, i);
					shapeCastInfo->ExcludedEntities.insert(entityID);
				}
			}

			SceneQueryHit tempHit;
			bool success = scene->GetPhysicsScene()->CastShape(shapeCastInfo, tempHit);

			if (success && inShapeCastData->RequiredComponentTypes != nullptr)
			{
				Entity entity = scene->GetEntityWithUUID(tempHit.HitEntity);
				size_t requiredComponentsCount = mono_array_length(inShapeCastData->RequiredComponentTypes);

				for (size_t i = 0; i < requiredComponentsCount; i++)
				{
					void* reflectionType = mono_array_get(inShapeCastData->RequiredComponentTypes, void*, i);
					if (reflectionType == nullptr)
					{
						ErrorWithTrace("Physics.Raycast - Why did you feel the need to pass a \"null\" as a required component?");
						success = false;
						break;
					}

					MonoType* componentType = mono_reflection_type_get_type((MonoReflectionType*)reflectionType);

#ifdef BEY_DEBUG
					MonoClass* typeClass = mono_type_get_class(componentType);
					MonoClass* parentClass = mono_class_get_parent(typeClass);

					bool validComponentFilter = parentClass != nullptr;
					if (validComponentFilter)
					{
						const char* parentClassName = mono_class_get_name(parentClass);
						const char* parentNameSpace = mono_class_get_namespace(parentClass);
						validComponentFilter = strstr(parentClassName, "Component") != nullptr && strstr(parentNameSpace, "Beyond") != nullptr;
					}

					if (!validComponentFilter)
					{
						ErrorWithTrace("Physics.Raycast - {0} does not inherit from Beyond.Component!", mono_class_get_name(typeClass));
						success = false;
						break;
					}
#endif

					if (!s_HasComponentFuncs[componentType](entity))
					{
						success = false;
						break;
					}
				}
			}

			if (success)
			{
				outHit->HitEntity = tempHit.HitEntity;
				outHit->Position = tempHit.Position;
				outHit->Normal = tempHit.Normal;
				outHit->Distance = tempHit.Distance;

				if (tempHit.HitCollider)
				{
					MonoObject* shapeInstance = nullptr;
					glm::vec3 offset(0.0f);
					
					switch (tempHit.HitCollider->GetType())
					{
						case ShapeType::Box:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<BoxColliderComponent>();
							offset = colliderComp.Offset;

							glm::vec3 halfSize = tempHit.HitCollider.As<BoxShape>()->GetHalfSize();
							CSharpInstance boxInstance("Beyond.BoxShape");
							shapeInstance = boxInstance.Instantiate(halfSize);
							break;
						}
						case ShapeType::Sphere:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<SphereColliderComponent>();
							offset = colliderComp.Offset;

							float radius = tempHit.HitCollider.As<SphereShape>()->GetRadius();
							CSharpInstance sphereInstance("Beyond.SphereShape");
							shapeInstance = sphereInstance.Instantiate(radius);
							break;
						}
						case ShapeType::Capsule:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<CapsuleColliderComponent>();
							offset = colliderComp.Offset;

							Ref<CapsuleShape> capsuleShape = tempHit.HitCollider.As<CapsuleShape>();
							float height = capsuleShape->GetHeight();
							float radius = capsuleShape->GetRadius();
							CSharpInstance capsuleInstance("Beyond.CapsuleShape");
							shapeInstance = capsuleInstance.Instantiate(height, radius);
							break;
						}
						case ShapeType::ConvexMesh:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							Ref<ConvexMeshShape> convexMeshShape = tempHit.HitCollider.As<ConvexMeshShape>();
							CSharpInstance meshInstance;
							CSharpInstance convexMeshInstance("Beyond.ConvexMeshShape");
							shapeInstance = convexMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
						case ShapeType::TriangleMesh:
						{
							Entity hitEntity = GetEntity(tempHit.HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							CSharpInstance triangleMeshInstance("Beyond.TriangleMeshShape");
							shapeInstance = triangleMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
					}

					if (shapeInstance != nullptr)
					{
						CSharpInstance colliderInstance("Beyond.Collider");
						outHit->HitCollider = colliderInstance.Instantiate(tempHit.HitEntity, shapeInstance, offset);
					}
				}
			}
			else
			{
				*outHit = ScriptRaycastHit();
			}

			hdelete shapeCastInfo;
			return success;
		}

		int32_t Physics_OverlapShape(ShapeQueryData* inOverlapData, MonoArray** outHits)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");

			if (inOverlapData->ShapeDataInstance == nullptr)
				return 0;

			CSharpInstanceInspector inspector(inOverlapData->ShapeDataInstance);
			BEY_CORE_VERIFY(inspector.InheritsFrom("Beyond.Shape"));

			ShapeOverlapInfo* shapeOverlapInfo = nullptr;

			ShapeType shapeType = inspector.GetFieldValue<ShapeType>("ShapeType");

			switch (shapeType)
			{
				case ShapeType::Box:
				{
					BoxOverlapInfo* boxOverlapInfo = hnew BoxOverlapInfo();
					boxOverlapInfo->HalfExtent = inspector.GetFieldValue<glm::vec3>("HalfExtent");
					shapeOverlapInfo = boxOverlapInfo;
					break;
				}
				case ShapeType::Sphere:
				{
					SphereOverlapInfo* sphereOverlapInfo = hnew SphereOverlapInfo();
					sphereOverlapInfo->Radius = inspector.GetFieldValue<float>("Radius");
					shapeOverlapInfo = sphereOverlapInfo;
					break;
				}
				case ShapeType::Capsule:
				{
					CapsuleOverlapInfo* capsuleOverlapInfo = hnew CapsuleOverlapInfo();
					capsuleOverlapInfo->HalfHeight = inspector.GetFieldValue<float>("HalfHeight");
					capsuleOverlapInfo->Radius = inspector.GetFieldValue<float>("Radius");
					shapeOverlapInfo = capsuleOverlapInfo;
					break;
				}
				case ShapeType::ConvexMesh:
				case ShapeType::TriangleMesh:
				case ShapeType::CompoundShape:
				case ShapeType::MutableCompoundShape:
				{
					WarnWithTrace("Can't do a shape overlap with Convex, Triangle or Compound shapes!");
					return false;
				}
			}

			if (shapeOverlapInfo == nullptr)
				return false;

			shapeOverlapInfo->Origin = inOverlapData->Origin;

			if (inOverlapData->ExcludeEntities)
			{
				size_t excludeEntitiesCount = mono_array_length(inOverlapData->ExcludeEntities);

				// NOTE: Same as calling the constructor with excludeEntitiesCount as the only argument
				shapeOverlapInfo->ExcludedEntities.rehash(excludeEntitiesCount);

				for (size_t i = 0; i < excludeEntitiesCount; i++)
				{
					uint64_t entityID = mono_array_get(inOverlapData->ExcludeEntities, uint64_t, i);
					shapeOverlapInfo->ExcludedEntities.insert(entityID);
				}
			}

			SceneQueryHit* hitArray;
			int32_t overlapCount = GetPhysicsScene()->OverlapShape(shapeOverlapInfo, &hitArray);

			if (overlapCount == 0)
				return 0;

			if (*outHits == nullptr)
				*outHits = ManagedArrayUtils::Create("Beyond.SceneQueryHit", uintptr_t(overlapCount));

			if (mono_array_length(*outHits) < overlapCount)
				ManagedArrayUtils::Resize(outHits, uintptr_t(overlapCount));

			if (overlapCount > 0 && inOverlapData->RequiredComponentTypes != nullptr)
			{
				for (size_t i = 0; i < size_t(overlapCount); i++)
				{
					Entity entity = scene->GetEntityWithUUID(hitArray[i].HitEntity);
					size_t requiredComponentsCount = mono_array_length(inOverlapData->RequiredComponentTypes);

					for (size_t i = 0; i < requiredComponentsCount; i++)
					{
						void* reflectionType = mono_array_get(inOverlapData->RequiredComponentTypes, void*, i);
						if (reflectionType == nullptr)
						{
							ErrorWithTrace("Physics.Raycast - Why did you feel the need to pass a \"null\" as a required component?");
							overlapCount = 0;
							break;
						}

						MonoType* componentType = mono_reflection_type_get_type((MonoReflectionType*)reflectionType);

#ifdef BEY_DEBUG
						MonoClass* typeClass = mono_type_get_class(componentType);
						MonoClass* parentClass = mono_class_get_parent(typeClass);

						bool validComponentFilter = parentClass != nullptr;
						if (validComponentFilter)
						{
							const char* parentClassName = mono_class_get_name(parentClass);
							const char* parentNameSpace = mono_class_get_namespace(parentClass);
							validComponentFilter = strstr(parentClassName, "Component") != nullptr && strstr(parentNameSpace, "Beyond") != nullptr;
						}

						if (!validComponentFilter)
						{
							ErrorWithTrace("Physics.Raycast - {0} does not inherit from Beyond.Component!", mono_class_get_name(typeClass));
							overlapCount = 0;
							break;
						}
#endif

						if (!s_HasComponentFuncs[componentType](entity))
						{
							overlapCount = 0;
							break;
						}
					}
				}
			}

			for (size_t i = 0; i < overlapCount; i++)
			{
				ScriptRaycastHit hitData;
				hitData.HitEntity = hitArray[i].HitEntity;
				hitData.Position = hitArray[i].Position;
				hitData.Normal = hitArray[i].Normal;
				hitData.Distance = hitArray[i].Distance;

				if (hitArray[i].HitCollider)
				{
					MonoObject* shapeInstance = nullptr;
					glm::vec3 offset(0.0f);

					switch (hitArray[i].HitCollider->GetType())
					{
						case ShapeType::Box:
						{
							Entity hitEntity = GetEntity(hitArray[i].HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<BoxColliderComponent>();
							offset = colliderComp.Offset;

							glm::vec3 halfSize = hitArray[i].HitCollider.As<BoxShape>()->GetHalfSize();
							CSharpInstance boxInstance("Beyond.BoxShape");
							shapeInstance = boxInstance.Instantiate(halfSize);
							break;
						}
						case ShapeType::Sphere:
						{
							Entity hitEntity = GetEntity(hitArray[i].HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<SphereColliderComponent>();
							offset = colliderComp.Offset;

							float radius = hitArray[i].HitCollider.As<SphereShape>()->GetRadius();
							CSharpInstance sphereInstance("Beyond.SphereShape");
							shapeInstance = sphereInstance.Instantiate(radius);
							break;
						}
						case ShapeType::Capsule:
						{
							Entity hitEntity = GetEntity(hitArray[i].HitEntity);
							const auto& colliderComp = hitEntity.GetComponent<CapsuleColliderComponent>();
							offset = colliderComp.Offset;

							Ref<CapsuleShape> capsuleShape = hitArray[i].HitCollider.As<CapsuleShape>();
							float height = capsuleShape->GetHeight();
							float radius = capsuleShape->GetRadius();
							CSharpInstance capsuleInstance("Beyond.CapsuleShape");
							shapeInstance = capsuleInstance.Instantiate(height, radius);
							break;
						}
						case ShapeType::ConvexMesh:
						{
							Entity hitEntity = GetEntity(hitArray[i].HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							Ref<ConvexMeshShape> convexMeshShape = hitArray[i].HitCollider.As<ConvexMeshShape>();
							CSharpInstance meshInstance;
							CSharpInstance convexMeshInstance("Beyond.ConvexMeshShape");
							shapeInstance = convexMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
						case ShapeType::TriangleMesh:
						{
							Entity hitEntity = GetEntity(hitArray[i].HitEntity);
							auto& colliderComp = hitEntity.GetComponent<MeshColliderComponent>();

							CSharpInstance meshBaseInstantiator("Beyond.MeshBase");
							MonoObject* meshBaseInstance = meshBaseInstantiator.Instantiate(colliderComp.ColliderAsset);

							CSharpInstance triangleMeshInstance("Beyond.TriangleMeshShape");
							shapeInstance = triangleMeshInstance.Instantiate(meshBaseInstance);
							break;
						}
					}

					if (shapeInstance != nullptr)
					{
						CSharpInstance colliderInstance("Beyond.Collider");
						hitData.HitCollider = colliderInstance.Instantiate(hitArray[i].HitEntity, shapeInstance, offset);
					}

					ManagedArrayUtils::SetValue(*outHits, uintptr_t(i), hitData);
				}
			}

			hdelete shapeOverlapInfo;
			return overlapCount;
		}

		void Physics_GetGravity(glm::vec3* outGravity)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");
			*outGravity = scene->GetPhysicsScene()->GetGravity();
		}

		void Physics_SetGravity(glm::vec3* inGravity)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");
			scene->GetPhysicsScene()->SetGravity(*inGravity);
		}

		void Physics_AddRadialImpulse(glm::vec3* inOrigin, float radius, float strength, EFalloffMode falloff, bool velocityChange)
		{
			Ref<Scene> scene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(scene, "No active scene!");
			scene->GetPhysicsScene()->AddRadialImpulse(*inOrigin, radius, strength, falloff, velocityChange);
		}

#pragma endregion

#pragma region Matrix4
		void Matrix4_LookAt(glm::vec3* eye, glm::vec3* center, glm::vec3* up, glm::mat4* outMatrix)
		{
			*outMatrix = glm::lookAt(*eye, *center, *up);
		}

#pragma endregion

#pragma region Noise
		
		Noise* Noise_Constructor(int seed)
		{
			return hnew Noise(seed);
		}

		void Noise_Destructor(Noise* _this)
		{
			hdelete _this;
		}

		float Noise_GetFrequency(Noise* _this) { return _this->GetFrequency(); }
		void Noise_SetFrequency(Noise* _this, float frequency) { _this->SetFrequency(frequency); }

		int Noise_GetFractalOctaves(Noise* _this) { return _this->GetFractalOctaves(); }
		void Noise_SetFractalOctaves(Noise* _this, int octaves) { _this->SetFractalOctaves(octaves); }

		float Noise_GetFractalLacunarity(Noise* _this) { return _this->GetFractalLacunarity(); }
		void Noise_SetFractalLacunarity(Noise* _this, float lacunarity) { _this->SetFractalLacunarity(lacunarity); }

		float Noise_GetFractalGain(Noise* _this) { return _this->GetFractalGain(); }
		void Noise_SetFractalGain(Noise* _this, float gain) { _this->SetFractalGain(gain); }

		float Noise_Get(Noise* _this, float x, float y) { return _this->Get(x, y); }

		void Noise_SetSeed(int seed) { Noise::SetSeed(seed); }
		float Noise_Perlin(float x, float y) { return Noise::PerlinNoise(x, y); }

#pragma endregion

#pragma region Log

		void Log_LogMessage(LogLevel level, MonoString* inFormattedMessage)
		{
			BEY_PROFILE_FUNC();

			eastl::string message = ScriptUtils::MonoStringToUTF8(inFormattedMessage);
			switch (level)
			{
				case LogLevel::Trace:
					BEY_CONSOLE_LOG_TRACE(message);
					break;
				case LogLevel::Debug:
					BEY_CONSOLE_LOG_INFO(message);
					break;
				case LogLevel::Info:
					BEY_CONSOLE_LOG_INFO(message);
					break;
				case LogLevel::Warn:
					BEY_CONSOLE_LOG_WARN(message);
					break;
				case LogLevel::Error:
					BEY_CONSOLE_LOG_ERROR(message);
					break;
				case LogLevel::Critical:
					BEY_CONSOLE_LOG_FATAL(message);
					break;
			}
		}

#pragma endregion

#pragma region Input

		bool Input_IsKeyPressed(KeyCode keycode) { return Input::IsKeyPressed(keycode); }
		bool Input_IsKeyHeld(KeyCode keycode) { return Input::IsKeyHeld(keycode); }
		bool Input_IsKeyDown(KeyCode keycode) { return Input::IsKeyDown(keycode); }
		bool Input_IsKeyReleased(KeyCode keycode) { return Input::IsKeyReleased(keycode); }

		bool Input_IsMouseButtonPressed(MouseButton button)
		{
			bool isPressed = Input::IsMouseButtonPressed(button);

			bool enableImGui = Application::Get().GetSpecification().EnableImGui;
			if (isPressed && enableImGui && GImGui->HoveredWindow != nullptr)
			{
				// Make sure we're in the viewport panel
				ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
				if (viewportWindow != nullptr)
					isPressed = GImGui->HoveredWindow->ID == viewportWindow->ID;
			}

			return isPressed;
		}
		bool Input_IsMouseButtonHeld(MouseButton button)
		{
			bool isHeld = Input::IsMouseButtonHeld(button);

			bool enableImGui = Application::Get().GetSpecification().EnableImGui;
			if (isHeld && enableImGui && GImGui->HoveredWindow != nullptr)
			{
				// Make sure we're in the viewport panel
				ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
				if (viewportWindow != nullptr)
					isHeld = GImGui->HoveredWindow->ID == viewportWindow->ID;
			}

			return isHeld;
		}
		bool Input_IsMouseButtonDown(MouseButton button)
		{
			bool isDown = Input::IsMouseButtonDown(button);

			bool enableImGui = Application::Get().GetSpecification().EnableImGui;
			if (isDown && enableImGui && GImGui->HoveredWindow != nullptr)
			{
				// Make sure we're in the viewport panel
				ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
				if (viewportWindow != nullptr)
					isDown = GImGui->HoveredWindow->ID == viewportWindow->ID;
			}

			return isDown;
		}
		bool Input_IsMouseButtonReleased(MouseButton button)
		{
			bool released = Input::IsMouseButtonReleased(button);

			bool enableImGui = Application::Get().GetSpecification().EnableImGui;
			if (released && enableImGui && GImGui->HoveredWindow != nullptr)
			{
				// Make sure we're in the viewport panel
				ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Viewport");
				if (viewportWindow != nullptr)
					released = GImGui->HoveredWindow->ID == viewportWindow->ID;
			}

			return released;
		}

		void Input_GetMousePosition(glm::vec2* outPosition)
		{
			auto [x, y] = Input::GetMousePosition();
			*outPosition = { x, y };
		}

		void Input_SetCursorMode(CursorMode mode) { Input::SetCursorMode(mode); }
		CursorMode Input_GetCursorMode() { return Input::GetCursorMode(); }
		bool Input_IsControllerPresent(int id) { return Input::IsControllerPresent(id); }

		MonoArray* Input_GetConnectedControllerIDs()
		{
			return ManagedArrayUtils::FromVector<int32_t>(Input::GetConnectedControllerIDs());
		}

		MonoString* Input_GetControllerName(int id)
		{
			auto name = Input::GetControllerName(id);
			if (name.empty())
				return ScriptUtils::EmptyMonoString();
			return ScriptUtils::UTF8StringToMono(&name.front());
		}

		bool Input_IsControllerButtonPressed(int id, int button) { return Input::IsControllerButtonPressed(id, button); }
		bool Input_IsControllerButtonHeld(int id, int button) { return Input::IsControllerButtonHeld(id, button); }
		bool Input_IsControllerButtonDown(int id, int button) { return Input::IsControllerButtonDown(id, button); }
		bool Input_IsControllerButtonReleased(int id, int button) { return Input::IsControllerButtonReleased(id, button); }


		float Input_GetControllerAxis(int id, int axis) { return Input::GetControllerAxis(id, axis); }
		uint8_t Input_GetControllerHat(int id, int hat) { return Input::GetControllerHat(id, hat); }

		float Input_GetControllerDeadzone(int id, int axis) { return Input::GetControllerDeadzone(id, axis); }
		void Input_SetControllerDeadzone(int id, int axis, float deadzone) { return Input::SetControllerDeadzone(id, axis, deadzone); }

#pragma endregion

#pragma region EditorUI

#ifndef BEY_DIST

		void EditorUI_Text(MonoString* inText)
		{
			eastl::string text = ScriptUtils::MonoStringToUTF8(inText);
			ImGui::TextUnformatted(text.c_str());
		}

		bool EditorUI_Button(MonoString* inLabel, glm::vec2* inSize)
		{
			eastl::string label = ScriptUtils::MonoStringToUTF8(inLabel);
			return ImGui::Button(label.c_str(), *((const ImVec2*)inSize));
		}

		bool EditorUI_BeginPropertyHeader(MonoString* label, bool openByDefault)
		{
			return UI::PropertyGridHeader(ScriptUtils::MonoStringToUTF8(label), openByDefault);
		}

		void EditorUI_EndPropertyHeader()
		{
			UI::EndTreeNode();
		}

		void EditorUI_PropertyGrid(bool inBegin)
		{
			if (inBegin)
				UI::BeginPropertyGrid();
			else
				UI::EndPropertyGrid();
		}

		bool EditorUI_PropertyFloat(MonoString* inLabel, float* outValue)
		{
			eastl::string label = ScriptUtils::MonoStringToUTF8(inLabel);
			return UI::Property(label.c_str(), *outValue);
		}

		bool EditorUI_PropertyVec2(MonoString* inLabel, glm::vec2* outValue)
		{
			eastl::string label = ScriptUtils::MonoStringToUTF8(inLabel);
			return UI::Property(label.c_str(), *outValue);
		}

		bool EditorUI_PropertyVec3(MonoString* inLabel, glm::vec3* outValue)
		{
			eastl::string label = ScriptUtils::MonoStringToUTF8(inLabel);
			return UI::Property(label.c_str(), *outValue);
		}

		bool EditorUI_PropertyVec4(MonoString* inLabel, glm::vec4* outValue)
		{
			eastl::string label = ScriptUtils::MonoStringToUTF8(inLabel);
			return UI::Property(label.c_str(), *outValue);
		}

#else
		void EditorUI_Text(MonoString* inText)
		{
		}

		bool EditorUI_Button(MonoString* inLabel, glm::vec2* inSize)
		{
			return false;
		}

		void EditorUI_PropertyGrid(bool inBegin)
		{
		}

		bool EditorUI_PropertyFloat(MonoString* inLabel, float* outValue)
		{
			return false;
		}

		bool EditorUI_PropertyVec2(MonoString* inLabel, glm::vec2* outValue)
		{
			return false;
		}

		bool EditorUI_PropertyVec3(MonoString* inLabel, glm::vec3* outValue)
		{
			return false;
		}

		bool EditorUI_PropertyVec4(MonoString* inLabel, glm::vec4* outValue)
		{
			return false;
		}

		bool EditorUI_BeginPropertyHeader(MonoString* label, bool openByDefault)
		{
			return false;
		}

		void EditorUI_EndPropertyHeader() {}

#endif
#pragma endregion

#pragma region SceneRenderer

		float SceneRenderer_GetOpacity()
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			return sceneRenderer->GetOpacity();
		}

		void SceneRenderer_SetOpacity(float opacity)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			sceneRenderer->SetOpacity(opacity);
		}

		bool SceneRenderer_DepthOfField_IsEnabled()
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			return sceneRenderer->GetDOFSettings().Enabled;
		}

		void SceneRenderer_DepthOfField_SetEnabled(bool enabled)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			sceneRenderer->GetDOFSettings().Enabled = enabled;
		}

		float SceneRenderer_DepthOfField_GetFocusDistance()
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			return sceneRenderer->GetDOFSettings().FocusDistance;
		}

		void SceneRenderer_DepthOfField_SetFocusDistance(float focusDistance)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			sceneRenderer->GetDOFSettings().FocusDistance = focusDistance;
		}

		float SceneRenderer_DepthOfField_GetBlurSize()
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			return sceneRenderer->GetDOFSettings().BlurSize;
		}

		void SceneRenderer_DepthOfField_SetBlurSize(float blurSize)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			BEY_CORE_VERIFY(sceneRenderer);
			sceneRenderer->GetDOFSettings().BlurSize = blurSize;
		}

#pragma endregion

#pragma region DebugRenderer

		void DebugRenderer_DrawLine(glm::vec3* p0, glm::vec3* p1, glm::vec4* color)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			Ref<DebugRenderer> debugRenderer = sceneRenderer->GetDebugRenderer();
			
			debugRenderer->DrawLine(*p0, *p1, *color);
		}

		void DebugRenderer_DrawQuadBillboard(glm::vec3* translation, glm::vec2* size, glm::vec4* color)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			Ref<DebugRenderer> debugRenderer = sceneRenderer->GetDebugRenderer();

			debugRenderer->DrawQuadBillboard(*translation, *size, *color);
		}

		void DebugRenderer_SetLineWidth(float width)
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			Ref<DebugRenderer> debugRenderer = sceneRenderer->GetDebugRenderer();

			debugRenderer->SetLineWidth(width);
		}

#pragma endregion


#pragma region PerformanceTimers

		float PerformanceTimers_GetFrameTime()
		{
			return (float)Application::Get().GetFrametime().GetMilliseconds();
		}

		float PerformanceTimers_GetGPUTime()
		{
			Ref<SceneRenderer> sceneRenderer = ScriptEngine::GetSceneRenderer();
			return (float)sceneRenderer->GetStatistics().TotalGPUTime;
		}

		float PerformanceTimers_GetMainThreadWorkTime()
		{
			return (float)Application::Get().GetPerformanceTimers().MainThreadWorkTime;
		}

		float PerformanceTimers_GetMainThreadWaitTime()
		{
			return (float)Application::Get().GetPerformanceTimers().MainThreadWaitTime;
		}

		float PerformanceTimers_GetRenderThreadWorkTime()
		{
			return (float)Application::Get().GetPerformanceTimers().RenderThreadWorkTime;
		}

		float PerformanceTimers_GetRenderThreadWaitTime()
		{
			return (float)Application::Get().GetPerformanceTimers().RenderThreadWaitTime;
		}

		uint32_t PerformanceTimers_GetFramesPerSecond()
		{
			return (uint32_t)(1.0f / (float)Application::Get().GetFrametime());
		}

		uint32_t PerformanceTimers_GetEntityCount()
		{
			Ref<Scene> activeScene = ScriptEngine::GetSceneContext();
			BEY_CORE_ASSERT(activeScene, "No active scene!");

			return (uint32_t) activeScene->GetEntityMap().size();
		}

		uint32_t PerformanceTimers_GetScriptEntityCount()
		{
			return (uint32_t)ScriptEngine::GetEntityInstances().size();
		}

#pragma endregion

	}

}
