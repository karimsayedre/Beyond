#pragma once

#include <glm/detail/type_quat.hpp>

#include "Beyond/Renderer/Camera.h"
#include "Beyond/Core/TimeStep.h"
#include "Beyond/Core/Events/KeyEvent.h"
#include "Beyond/Core/Events/MouseEvent.h"

namespace Beyond {

	enum class CameraMode
	{
		NONE, FLYCAM, ARCBALL
	};

	class EditorCamera : public Camera
	{
	public:
		EditorCamera(const float degFov, const float width, const float height, const float nearP, const float farP);
		void Init();

		void Focus(const glm::vec3& focusPoint);
		void OnUpdate(Timestep ts);
		void OnEvent(Event& e);

		bool IsActive() const { return m_IsActive; }
		void SetActive(const bool active) { m_IsActive = active; }

		bool IsMoving() const override;

		[[nodiscard]] CameraMode GetCurrentMode() const { return m_CameraMode; }

		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(const float distance) { m_Distance = distance; }

		[[nodiscard]] const glm::vec3& GetFocalPoint() const { return m_FocalPoint; }

		inline void SetViewportSize(const uint32_t width, const uint32_t height)
		{
			if (m_ViewportWidth == width && m_ViewportHeight == height)
				return;
			SetPerspectiveProjectionMatrix(m_VerticalFOV, (float)width, (float)height, m_NearClip, m_FarClip);
			m_ViewportWidth = width;
			m_ViewportHeight = height;
		}

		inline void SetFOV(float fov) // In radians
		{
			//if (glm::abs(m_VerticalFOV - fov) < 0.0001f)
				//return;
			SetPerspectiveProjectionMatrix(fov, (float)m_ViewportWidth, (float)m_ViewportHeight, m_NearClip, m_FarClip);
			m_VerticalFOV = fov;
		}

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjection() const { return GetProjectionMatrix() * m_ViewMatrix; }
		glm::mat4 GetUnReversedViewProjection() const { return GetUnReversedProjectionMatrix() * m_ViewMatrix; }

		glm::vec3 GetUpDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetForwardDirection() const;

		const glm::vec3& GetPosition() const { return m_Position; }

		glm::quat GetOrientation() const;

		[[nodiscard]] float GetVerticalFOV() const { return m_VerticalFOV; }
		[[nodiscard]] float GetAspectRatio() const { return m_AspectRatio; }
		[[nodiscard]] float GetNearClip() const { return m_NearClip; }
		[[nodiscard]] float GetFarClip() const { return m_FarClip; }
		[[nodiscard]] float GetPitch() const { return m_Pitch; }
		[[nodiscard]] float GetYaw() const { return m_Yaw; }
		[[nodiscard]] float GetCameraSpeed() const;
	private:
		void UpdateCameraView();

		bool OnMouseScroll(const MouseScrolledEvent& e);

		void MousePan(const glm::vec2& delta);
		void MouseRotate(const glm::vec2& delta);
		void MouseZoom(float delta);

		glm::vec3 CalculatePosition() const;

		std::pair<float, float> PanSpeed() const;
		float RotationSpeed() const;
		float ZoomSpeed() const;
	private:
		glm::mat4 m_ViewMatrix;
		glm::vec3 m_Position, m_Direction, m_FocalPoint;

		// Perspective projection params
		float m_VerticalFOV, m_AspectRatio, m_NearClip, m_FarClip;

		bool m_IsActive = false;
		mutable bool m_IsMoving = false;

		glm::vec2 m_InitialMousePosition {};
		glm::vec3 m_InitialFocalPoint, m_InitialRotation;

		float m_Distance;
		float m_NormalSpeed{ 0.002f };

		float m_Pitch, m_Yaw; //rad
		float m_PitchDelta{}, m_YawDelta{};
		glm::vec3 m_PositionDelta{};
		glm::vec3 m_RightDirection{};

		CameraMode m_CameraMode{ CameraMode::ARCBALL };

		float m_MinFocusDistance{ 100.0f };

		uint32_t m_ViewportWidth{ 1280 }, m_ViewportHeight{ 720 };

		constexpr static float MIN_SPEED{ 0.00001f }, MAX_SPEED{ 2.0f };
		friend class EditorLayer;
	};

}
