//
// Note:	this file is to be included in client applications ONLY
//			NEVER include this file anywhere in the engine codebase
//
#pragma once

#include "Beyond/Core/Application.h"
#include "Beyond/Core/Log.h"
#include "Beyond/Core/Input.h"
#include "Beyond/Core/TimeStep.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Core/Platform.h"

#include "Beyond/Core/Events/Event.h"
#include "Beyond/Core/Events/ApplicationEvent.h"
#include "Beyond/Core/Events/KeyEvent.h"
#include "Beyond/Core/Events/MouseEvent.h"
#include "Beyond/Core/Events/SceneEvents.h"

#include "Beyond/Core/Math/AABB.h"
#include "Beyond/Core/Math/Ray.h"

#include "imgui/imgui.h"

// --- Beyond Render API ------------------------------
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Renderer/SceneRenderer.h"
#include "Beyond/Renderer/RenderPass.h"
#include "Beyond/Renderer/Framebuffer.h"
#include "Beyond/Renderer/VertexBuffer.h"
#include "Beyond/Renderer/IndexBuffer.h"
#include "Beyond/Renderer/RasterPipeline.h"
#include "Beyond/Renderer/Texture.h"
#include "Beyond/Renderer/Shader.h"
#include "Beyond/Renderer/Mesh.h"
#include "Beyond/Renderer/Camera.h"
#include "Beyond/Renderer/Material.h"
// ---------------------------------------------------

// Scenes
#include "Beyond/Scene/Scene.h"
#include "Beyond/Scene/SceneCamera.h"
#include "Beyond/Scene/SceneSerializer.h"
#include "Beyond/Scene/Components.h"
