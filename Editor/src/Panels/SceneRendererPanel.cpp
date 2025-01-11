#include "SceneRendererPanel.h"

#include <ranges>
#include <examples/imgui_impl_vulkan_with_textures.h>

#include "Beyond/Renderer/SceneRenderer.h"
#include "Beyond/Core/Application.h"
#include "Beyond/Renderer/Renderer.h"
#include "Beyond/Debug/Profiler.h"
#include "Beyond/ImGui/ImGui.h"

#include "imgui/imgui.h"

namespace Beyond {

	bool SceneRendererPanel::OnImGuiRender(bool& isOpen)
	{
		BEY_PROFILE_FUNC();
		bool changed = false;
		if (ImGui::Begin("Scene Renderer", &isOpen) && m_Context)
		{
			auto& options = m_Context->m_Options;

			ImGui::Text("Viewport Size: %d, %d", m_Context->m_RenderWidth, m_Context->m_RenderHeight);
			ImGui::Text("Target Size: %d, %d", m_Context->m_TargetWidth, m_Context->m_TargetHeight);

			bool vsync = Application::Get().GetWindow().IsVSync();
			if (UI::Checkbox("Vsync", &vsync))
				Application::Get().GetWindow().SetVSync(vsync);

			const float size = ImGui::GetContentRegionAvail().x;
			ImGui::SameLine(size - 100);

			if (UI::Button("Save Screenshot"))
			{
				Ref<Image2D> image = m_Context->GetFinalPassImage();
				image->SaveImageToFile("Screenshots");
			}

			if (UI::BeginTreeNode("Debug", true))
			{
				if (m_Context->m_ResourcesCreated)
				{
					float size = ImGui::GetContentRegionAvail().x;

					const ImVec2 imageSize = { size, size * ((float)m_Context->m_TargetHeight / (float)m_Context->m_TargetWidth) };

					size = 2048.0f;
					const ImVec2 toolTipSize = { size, size * ((float)m_Context->m_TargetHeight / (float)m_Context->m_TargetWidth) };

					if (UI::BeginTreeNode("Debug Image", true))
					{
						UI::Image(m_Context->m_DebugImage, imageSize, { 0, 1 }, { 1, 0 });
						if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
						{
							ImGui::BeginTooltip();
							UI::Image(m_Context->m_DebugImage, toolTipSize, { 0, 1 }, { 1, 0 });
							ImGui::EndTooltip();
						}
						UI::EndTreeNode();
					}

					if (UI::BeginTreeNode("Geometry Motion Vectors", false))
					{
						if (m_Context->m_ResourcesCreated)
						{
							const float size = ImGui::GetContentRegionAvail().x;
							UI::Image(m_Context->m_PreDepthPass->GetOutput(0), imageSize, { 0, 1 }, { 1, 0 });
							if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
							{
								ImGui::BeginTooltip();
								UI::Image(m_Context->m_PreDepthPass->GetOutput(0), toolTipSize, { 0, 1 }, { 1, 0 });
								ImGui::EndTooltip();
							}
						}
						UI::EndTreeNode();
					}

					if (UI::BeginTreeNode("Resloved Motion Vectors", false))
					{
						UI::Image(m_Context->m_ExposureImage, imageSize, { 0, 1 }, { 1, 0 });
						if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Right))
						{
							ImGui::BeginTooltip();
							UI::Image(m_Context->m_ExposureImage, toolTipSize, { 0, 1 }, { 1, 0 });
							ImGui::EndTooltip();
						}
						UI::EndTreeNode();
					}
				}
				UI::EndTreeNode();
			}

			const float headerSpacingOffset = -(ImGui::GetStyle().ItemSpacing.y + 1.0f);
			const bool shadersTreeNode = UI::PropertyGridHeader("Shaders", true);

			// TODO: Better placement of the button?
			const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::SameLine(ImGui::GetWindowWidth() - lineHeight * 4.0f);
			if (UI::Button("Reload All"))
			{
				for (auto& shaders : Renderer::GetShaderLibrary()->GetShaders() | std::views::values)
					for (auto& shader : shaders)
						shader->Reload(true);
				Renderer::NotifyShaderUpdate();
				changed = true;
			}
			if (shadersTreeNode)
			{
				static std::string searchedString = "Path";
				UI::Widgets::SearchWidget(searchedString);
				ImGui::Indent();
				for (auto& [name, shaders] : Renderer::GetShaderLibrary()->GetShaders())
				{
					for (auto& shader : shaders)
					{
						if (!UI::IsMatchingSearch(name, searchedString))
							continue;

						ImGui::Columns(2);
						ImGui::Text(name.c_str());
						ImGui::NextColumn();
						std::string buttonName = fmt::format("Reload##{0}", name);
						if (ImGui::Button(buttonName.c_str()))
						{
							shader->Reload(true);
							Renderer::NotifyShaderUpdate();
							changed = true;
						}
						ImGui::Columns(1);
					}
				}

				ImGui::Unindent();
				UI::EndTreeNode();
				UI::ShiftCursorY(18.0f);
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);


			if (UI::PropertyGridHeader("Ray Tracing"))
			{
				UI::BeginPropertyGrid();

				const static char* modes[] = { "Disabled", "Ray Tracing", "Path Tracing", "Restir", "Restir-Compute" };

				static int lastMode = (int)SceneRenderer::RaytracingMode::Pathtracing;
				int modeIndex = (int)m_Context->m_RaytracingSettings.Mode;

				if (UI::PropertyDropdown("Rendering Mode", modes, 5, modeIndex))
				{
					//if (m_Context->m_RaytracingSettings.Mode > SceneRenderer::RaytracingMode::None)
					lastMode = (int)m_Context->m_RaytracingSettings.Mode;
					m_Context->m_RaytracingSettings.Mode = (SceneRenderer::RaytracingMode)modeIndex;
					BEY_CORE_INFO_TAG("Renderer", "Changed Rendering mode to: {}", magic_enum::enum_name<SceneRenderer::RaytracingMode>(m_Context->m_RaytracingSettings.Mode));
					changed = true;
				}

				UI::ShiftCursor(10.0f, 9.0f);
				ImGui::Text("Change to last mode");
				ImGui::NextColumn();
				if (UI::Button("Last Mode"))
				{
					std::swap(lastMode, *(int*)&m_Context->m_RaytracingSettings.Mode);
					BEY_CORE_INFO_TAG("Renderer", "Changed Rendering mode to: {}", magic_enum::enum_name<SceneRenderer::RaytracingMode>(m_Context->m_RaytracingSettings.Mode));
					changed = true;
				}

				if (m_Context->m_RaytracingSettings.Mode == SceneRenderer::RaytracingMode::Pathtracing || m_Context->m_RaytracingSettings.Mode == SceneRenderer::RaytracingMode::Restir ||
					m_Context->m_RaytracingSettings.Mode == SceneRenderer::RaytracingMode::RestirComp)
				{
					changed |= UI::Property("Max Accumulated Frames", m_Context->m_RaytracingSettings.MaxFrames, 0, UINT_MAX);
					changed |= UI::Property("Russian Roulette", m_Context->m_RaytracingSettings.EnableRussianRoulette);
					ImGui::Text("Accumulated Frames: ");
					ImGui::NextColumn();
					ImGui::Text("%d", m_Context->m_AccumulatedFrames);
					ImGui::NextColumn();
					ImGui::Text("Accumulated Path Tracing Frames: ");
					ImGui::NextColumn();
					ImGui::Text("%d", m_Context->m_AccumulatedPathtracingFrames);
					ImGui::NextColumn();
				}

				if (m_Context->m_RaytracingSettings.Mode == SceneRenderer::RaytracingMode::RestirComp)
				{
					changed |= UI::PropertyPowerOfTwo("Work Group Size", m_Context->m_RaytracingSettings.WorkGroupSize, 1, 32);
				}

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Composite Settings"))
			{
				UI::BeginPropertyGrid();

				const static char* modes[] = { "None", "PBR Neutral", "ACES","AGX" };

				int modeIndex = (int)m_Context->m_CompositeSettings.Tonemapper;
				if (UI::PropertyDropdown("Tone Mapper", modes, 4, modeIndex))
				{
					m_Context->m_CompositeSettings.Tonemapper = modeIndex;
				}

				UI::Property("Grain Strength", m_Context->m_CompositeSettings.GrainStrength, 0.1f, 0.0f, 50.0f);
				//UI::Property("Opacity", m_Context->m_CompositeSettings.Opacity, 0.1f, 0.0f, 1.0f);

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);


			if (UI::PropertyGridHeader("Deep Learning Super-Sampling"))
			{
				UI::BeginPropertyGrid();

				bool resetDLSS = false;
				resetDLSS |= UI::Property("Enable", m_Context->m_DLSSSettings.Enable);
				resetDLSS |= UI::Property("Enable Fake DLSS", m_Context->m_DLSSSettings.FakeDLSS);
				resetDLSS |= UI::Property("Enable Jitter", m_Context->m_DLSSSettings.EnableJitter);
				resetDLSS |= UI::Property("Is HDR", m_Context->m_DLSSSettings.IsHDR);
				resetDLSS |= UI::Property("Base Sample Count", m_Context->m_DLSSSettings.BasePhases, 0, 32, "Samples before repeating the jitter sequence.");
				resetDLSS |= UI::Property("Use Quadrant Jitter", m_Context->m_DLSSSettings.UseQuadrants);
				resetDLSS |= UI::Property("Quadrant", m_Context->m_DLSSSettings.Quadrant, 0, 4, "4 is a cycle between quadrants.");
				resetDLSS |= UI::Property("Jittered Motion Vectors", m_Context->m_DLSSSettings.JitteredMotionVectors);


				/*
				 *     NVSDK_NGX_PerfQuality_Value_MaxPerf,
	NVSDK_NGX_PerfQuality_Value_Balanced,
	NVSDK_NGX_PerfQuality_Value_MaxQuality,
	// Extended PerfQuality modes
	NVSDK_NGX_PerfQuality_Value_UltraPerformance,
	NVSDK_NGX_PerfQuality_Value_UltraQuality,
	NVSDK_NGX_PerfQuality_Value_DLAA,
				 */

				const static char* modes[] = { "Max Performance", "Balanced", "Max Quality", "Ultra Performance", "DLAA" };

				int modeIndex = (int)m_Context->m_DLSSSettings.Mode;
				static int lastMode = modeIndex;

				if (UI::PropertyDropdown("Quality Mode", modes, 5, modeIndex))
				{
					lastMode = (int)m_Context->m_DLSSSettings.Mode;
					m_Context->m_DLSSSettings.Mode = (DLSSQualityValue)modeIndex;
					resetDLSS = true;
				}

				m_Context->m_NeedsResize |= resetDLSS;
				changed |= resetDLSS;

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Dynamic-Diffuse Global Illumination", false))
			{
				UI::BeginPropertyGrid();

				UI::Property("Enable", m_Context->m_DDGISettings.Enable);
				UI::Property("Enable Probe Visualization", m_Context->m_DDGISettings.ProbeVis);
				UI::Property("Enable Texture Visualization", m_Context->m_DDGISettings.TextureVis);

				if (m_Context->m_DDGISettings.TextureVis)
				{
					if (UI::BeginTreeNode("Texture Visualization"))
					{
						UI::Property("Instance Offset", m_Context->m_DDGITextureVisSettings.InstanceOffset);
						UI::Property("Probe Type", m_Context->m_DDGITextureVisSettings.ProbeType);
						UI::Property("Probe Radius", m_Context->m_DDGITextureVisSettings.ProbeRadius);
						UI::Property("Distance Divisor", m_Context->m_DDGITextureVisSettings.DistanceDivisor);
						UI::Property("Ray Data Texture Scale", m_Context->m_DDGITextureVisSettings.RayDataTextureScale);
						UI::Property("Irradiance Texture Scale", m_Context->m_DDGITextureVisSettings.IrradianceTextureScale);
						UI::Property("Distance Texture Scale", m_Context->m_DDGITextureVisSettings.DistanceTextureScale);
						UI::Property("Probe Data Texture Scale", m_Context->m_DDGITextureVisSettings.ProbeDataTextureScale);
						UI::Property("Probe Variability Texture Scale", m_Context->m_DDGITextureVisSettings.ProbeVariabilityTextureScale);
						UI::EndTreeNode();

					}
				}

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Render Statistics"))
			{
				const auto& commandBuffer = m_Context->m_MainCommandBuffer;
				const auto& gpuTimeQueries = m_Context->m_GPUTimeQueries;

				uint32_t frameIndex = Renderer::RT_GetCurrentFrameIndex();
				ImGui::Text("GPU time: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, 0, true));

				if (UI::BeginTreeNode("VRAM Statistics"))
				{
					const auto vramStats = Renderer::GetGPUMemoryStats();
					ImGui::Text("VRAM Heap Usage: %s", Utils::BytesToString(vramStats.Used).c_str());
					ImGui::Text("VRAM Heap Budget: %s", Utils::BytesToString(vramStats.TotalAvailable).c_str());
					ImGui::Text("VRAM Buffer Allocation Count: %d", vramStats.BufferAllocationCount);
					ImGui::Text("VRAM Buffer Allocation Size: %s", Utils::BytesToString(vramStats.BufferAllocationSize).c_str());
					ImGui::Text("VRAM Image Allocation Count: %d", vramStats.ImageAllocationSize);
					ImGui::Text("VRAM Image Allocation Size: %s", Utils::BytesToString(vramStats.ImageAllocationCount).c_str());
					UI::EndTreeNode();
				}
				else
					UI::ShiftCursorY(headerSpacingOffset);

				if (UI::BeginTreeNode("GPU Time Statistics"))
				{
					//ImGui::Text("Building Acceleration Structures: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.BuildAccelerationStructuresQuery));
					ImGui::Text("Dir Shadow Map Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.DirShadowMapPassQuery));
					ImGui::Text("Spot Shadow Map Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.SpotShadowMapPassQuery));
					ImGui::Text("Depth Pre-Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.DepthPrePassQuery));
					ImGui::Text("Hierarchical Depth: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.HierarchicalDepthQuery));
					ImGui::Text("Motion Vectors: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.MotionVectorsQuery));
					ImGui::Text("Pre-Integration: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.PreIntegrationQuery));
					ImGui::Text("Light Culling Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.LightCullingPassQuery));
					ImGui::Text("Geometry Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.GeometryPassQuery));
					ImGui::Text("Ray tracing Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.RaytracingQuery));
					ImGui::Text("Pre-Convoluted Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.PreConvolutionQuery));
					ImGui::Text("GTAO Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.GTAOPassQuery));
					ImGui::Text("GTAO Denoise Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.GTAODenoisePassQuery));
					ImGui::Text("AO Composite Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.AOCompositePassQuery));
					ImGui::Text("DLSS Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.DLSSPassQuery));
					ImGui::Text("Bloom Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.BloomComputePassQuery));
					ImGui::Text("SSR Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.SSRQuery));
					ImGui::Text("SSR Composite Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.SSRCompositeQuery));
					ImGui::Text("Jump Flood Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.JumpFloodPassQuery));
					ImGui::Text("Composite Pass: %.3fms", commandBuffer->GetExecutionGPUTime(frameIndex, gpuTimeQueries.CompositePassQuery));

					UI::EndTreeNode();
				}
				else
					UI::ShiftCursorY(headerSpacingOffset);

				if (UI::BeginTreeNode("Pipeline Statistics"))
				{
					const PipelineStatistics& pipelineStats = commandBuffer->GetPipelineStatistics(frameIndex);
					ImGui::Text("Input Assembly Vertices: %llu", pipelineStats.InputAssemblyVertices);
					ImGui::Text("Input Assembly Primitives: %llu", pipelineStats.InputAssemblyPrimitives);
					ImGui::Text("Vertex Shader Invocations: %llu", pipelineStats.VertexShaderInvocations);
					ImGui::Text("Clipping Invocations: %llu", pipelineStats.ClippingInvocations);
					ImGui::Text("Clipping Primitives: %llu", pipelineStats.ClippingPrimitives);
					ImGui::Text("Fragment Shader Invocations: %llu", pipelineStats.FragmentShaderInvocations);
					ImGui::Text("Compute Shader Invocations: %llu", pipelineStats.ComputeShaderInvocations);
					UI::EndTreeNode();
				}

				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Visualization", false))
			{
				UI::BeginPropertyGrid();
				UI::Property("Show Light Complexity", m_Context->RendererDataUB.ShowLightComplexity);
				UI::Property("Show Shadow Cascades", m_Context->RendererDataUB.ShowCascades);
				// static int maxDrawCall = 1000;
				// UI::PropertySlider("Selected Draw", VulkanRenderer::GetSelectedDrawCall(), -1, maxDrawCall);
				// UI::Property("Max Draw Call", maxDrawCall);
				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

#if 0
			if (UI::PropertyGridHeader("Edge Detection"))
			{
				const float size = ImGui::GetContentRegionAvail().x;
				UI::Image(m_Context->m_EdgeDetectionPipeline->GetSpecification().RenderPass->GetSpecification().TargetFramebuffer->GetImage(), { size, size * (0.9f / 1.6f) }, { 0, 1 }, { 1, 0 });

				UI::EndTreeNode();
			}
#endif

			if (UI::PropertyGridHeader("Ambient Occlusion"))
			{
				UI::BeginPropertyGrid();
				UI::Property("ShadowTolerance", options.AOShadowTolerance, 0.001f, 0.0f, 1.0f);
				UI::EndPropertyGrid();

				if (UI::BeginTreeNode("Ground-Truth Ambient Occlusion"))
				{
					UI::BeginPropertyGrid();
					if (UI::Property("Enable", options.EnableGTAO))
					{
						if (!options.EnableGTAO)
							*(int*)&options.ReflectionOcclusionMethod &= ~(int)ShaderDef::AOMethod::GTAO;

						// NOTE: updating shader defines causes Vulkan render pass issues, and we don't need them here any more,
						//            but probably worth looking into at some point
						// Renderer::SetGlobalMacroInShaders("__BEY_AO_METHOD", fmt::format("{}", ShaderDef::GetAOMethod(options.EnableGTAO)));
						// Renderer::SetGlobalMacroInShaders("__BEY_REFLECTION_OCCLUSION_METHOD", std::to_string((int)options.ReflectionOcclusionMethod));
					}

					UI::Property("Radius", m_Context->GTAODataCB.EffectRadius, 0.001f, 0.1f, 10000.f);
					UI::Property("Falloff Range", m_Context->GTAODataCB.EffectFalloffRange, 0.001f, 0.01f, 1.0f);
					UI::Property("Radius Multiplier", m_Context->GTAODataCB.RadiusMultiplier, 0.001f, 0.3f, 3.0f);
					UI::Property("Sample Distribution Power", m_Context->GTAODataCB.SampleDistributionPower, 0.001f, 1.0f, 3.0f);
					UI::Property("Thin Occluder Compensation", m_Context->GTAODataCB.ThinOccluderCompensation, 0.001f, 0.0f, 0.7f);
					UI::Property("Final Value Power", m_Context->GTAODataCB.FinalValuePower, 0.001f, 0.0f, 5.f);
					UI::Property("Denoise Blur Beta", m_Context->GTAODataCB.DenoiseBlurBeta, 0.001f, 0.0f, 30.f);
					UI::Property("Depth MIP Sampling Offset", m_Context->GTAODataCB.DepthMIPSamplingOffset, 0.005f, 0.0f, 30.f);
					UI::PropertySlider("Denoise Passes", options.GTAODenoisePasses, 0, 8);
					if (UI::Property("Half Resolution", m_Context->GTAODataCB.HalfRes))
						m_Context->m_NeedsResize = true;
					UI::EndPropertyGrid();

					UI::EndTreeNode();
				}

				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("SSR", false))
			{
				auto& ssrOptions = m_Context->m_SSROptions;

				UI::BeginPropertyGrid();
				UI::Property("Enable SSR", options.EnableSSR);
				UI::Property("Enable Cone Tracing", ssrOptions.EnableConeTracing, "Enable rough reflections.");

				const static char* aoMethods[4] = { "Disabled", "Ground-Truth Ambient Occlusion", "Horizon-Based Ambient Occlusion", "All" };

				//TODO: Disable disabled methods in ImGui
				int methodIndex = ShaderDef::GetMethodIndex(options.ReflectionOcclusionMethod);
				if (UI::PropertyDropdown("Reflection Occlusion method", aoMethods, 4, methodIndex))
				{
					options.ReflectionOcclusionMethod = ShaderDef::ROMETHODS[methodIndex];
					if ((int)options.ReflectionOcclusionMethod & (int)ShaderDef::AOMethod::GTAO)
						options.EnableGTAO = true;
					Renderer::SetGlobalMacroInShaders("__BEY_REFLECTION_OCCLUSION_METHOD", fmt::format("{}", (int)(options.ReflectionOcclusionMethod)));
					Renderer::SetGlobalMacroInShaders("__BEY_AO_METHOD", fmt::format("{}", (int)ShaderDef::GetAOMethod(options.EnableGTAO)));
				}

				UI::Property("Brightness", ssrOptions.Brightness, 0.001f, 0.0f, 1.0f);
				UI::Property("Depth Tolerance", ssrOptions.DepthTolerance, 0.01f, 0.0f, std::numeric_limits<float>::max());
				UI::Property("Roughness Depth Tolerance", ssrOptions.RoughnessDepthTolerance, 0.33f, 0.0f, std::numeric_limits<float>::max(),
					"The higher the roughness the higher the depth tolerance.\nWorks best with cone tracing enabled.\nReduce as much as possible.");
				UI::Property("Horizontal Fade In", ssrOptions.FadeIn.x, 0.005f, 0.0f, 10.0f);
				UI::Property("Vertical Fade In", ssrOptions.FadeIn.y, 0.005f, 0.0f, 10.0f);
				UI::Property("Facing Reflections Fading", ssrOptions.FacingReflectionsFading, 0.01f, 0.0f, 2.0f);
				UI::Property("Luminance Factor", ssrOptions.LuminanceFactor, 0.001f, 0.0f, 2.0f, "Can break energy conservation law!");
				UI::PropertySlider("Maximum Steps", ssrOptions.MaxSteps, 1, 200);
				if (UI::Property("Half Resolution", ssrOptions.HalfRes))
					m_Context->m_NeedsResize = true;
				UI::EndPropertyGrid();

				if (UI::BeginTreeNode("Debug Views", false))
				{
					if (m_Context->m_ResourcesCreated)
					{
						const float size = ImGui::GetContentRegionAvail().x;
						UI::Image(m_Context->m_SSRImage, { size, size * (0.9f / 1.6f) }, { 0, 1 }, { 1, 0 });
						static int32_t mip = 0;
						UI::PropertySlider("Pre-convoluted Mip", mip, 0, (int)m_Context->m_PreConvolutedTexture.Texture->GetMipLevelCount() - 1);
						UI::ImageMip(m_Context->m_PreConvolutedTexture.Texture->GetImage(), mip, { size, size * (0.9f / 1.6f) }, { 0, 1 }, { 1, 0 });

					}
					UI::EndTreeNode();
				}
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Bloom Settings"))
			{
				auto& bloomSettings = m_Context->m_BloomSettings;

				UI::BeginPropertyGrid();
				UI::Property("Bloom Enabled", bloomSettings.Enabled);
				UI::Property("Threshold", bloomSettings.Threshold);
				UI::Property("Knee", bloomSettings.Knee);
				UI::Property("Upsample Scale", bloomSettings.UpsampleScale);
				UI::Property("Intensity", bloomSettings.Intensity, 0.05f, 0.0f, 20.0f);
				UI::Property("Dirt Intensity", bloomSettings.DirtIntensity, 0.05f, 0.0f, 20.0f);

				// TODO: move this to somewhere else
				UI::Image(m_Context->m_BloomDirtTexture, ImVec2(64, 64));
				if (ImGui::IsItemHovered())
				{
					if (ImGui::IsItemClicked())
					{
						std::string filename = FileSystem::OpenFileDialog().string();
						if (!filename.empty())
							m_Context->m_BloomDirtTexture = Texture2D::Create(TextureSpecification(), filename);
					}
				}

				UI::EndPropertyGrid();
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Depth of Field", false))
			{
				auto& dofSettings = m_Context->m_DOFSettings;

				UI::BeginPropertyGrid();
				UI::Property("DOF Enabled", dofSettings.Enabled);
				if (UI::Property("Focus Distance", dofSettings.FocusDistance, 0.1f, 0.0f, std::numeric_limits<float>::max()))
					dofSettings.FocusDistance = glm::max(dofSettings.FocusDistance, 0.0f);
				if (UI::Property("Blur Size", dofSettings.BlurSize, 0.025f, 0.0f, 20.0f))
					dofSettings.BlurSize = glm::clamp(dofSettings.BlurSize, 0.0f, 20.0f);
				UI::EndPropertyGrid();

				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

			if (UI::PropertyGridHeader("Shadows", false))
			{
				auto& rendererDataUB = m_Context->RendererDataUB;

				UI::BeginPropertyGrid();
				changed |= UI::Property("Soft Shadows", rendererDataUB.SoftShadows);
				changed |= UI::Property("DirLight Size", rendererDataUB.Range, 0.01f);
				changed |= UI::Property("Max Shadow Distance", rendererDataUB.MaxShadowDistance, 1.0f);
				changed |= UI::Property("Shadow Fade", rendererDataUB.ShadowFade, 5.0f);
				UI::EndPropertyGrid();
				if (UI::BeginTreeNode("Cascade Settings"))
				{
					UI::BeginPropertyGrid();
					changed |= UI::Property("Cascade Fading", rendererDataUB.CascadeFading);
					changed |= UI::Property("Cascade Transition Fade", rendererDataUB.CascadeTransitionFade, 0.05f, 0.0f, FLT_MAX);
					changed |= UI::Property("Cascade Split", m_Context->CascadeSplitLambda, 0.01f);
					changed |= UI::Property("CascadeNearPlaneOffset", m_Context->CascadeNearPlaneOffset, 0.1f, -FLT_MAX, 0.0f);
					changed |= UI::Property("CascadeFarPlaneOffset", m_Context->CascadeFarPlaneOffset, 0.1f, 0.0f, FLT_MAX);
					changed |= UI::Property("ScaleShadowToOrigin", m_Context->m_ScaleShadowCascadesToOrigin, 0.025f, 0.0f, 1.0f);
					changed |= UI::Property("Use manual cascade splits", m_Context->m_UseManualCascadeSplits);
					if (m_Context->m_UseManualCascadeSplits)
					{
						changed |= UI::Property("Cascade 0", m_Context->m_ShadowCascadeSplits[0], 0.025f, 0.0f);
						changed |= UI::Property("Cascade 1", m_Context->m_ShadowCascadeSplits[1], 0.025f, 0.0f);
						changed |= UI::Property("Cascade 2", m_Context->m_ShadowCascadeSplits[2], 0.025f, 0.0f);
						changed |= UI::Property("Cascade 3", m_Context->m_ShadowCascadeSplits[3], 0.025f, 0.0f);

					}
					UI::EndPropertyGrid();
					UI::EndTreeNode();
				}
				if (UI::BeginTreeNode("Shadow Map", false))
				{
					static int cascadeIndex = 0;
					auto fb = m_Context->m_DirectionalShadowMapPass[cascadeIndex]->GetTargetFramebuffer();
					auto image = fb->GetDepthImage();

					float size = ImGui::GetContentRegionAvail().x; // (float)fb->GetWidth() * 0.5f, (float)fb->GetHeight() * 0.5f
					UI::BeginPropertyGrid();
					UI::PropertySlider("Cascade Index", cascadeIndex, 0, 3);
					UI::EndPropertyGrid();
					if (m_Context->m_ResourcesCreated)
					{
						UI::Image(image, (uint32_t)cascadeIndex, { size, size }, { 0, 1 }, { 1, 0 });
					}
					UI::EndTreeNode();
				}

				if (UI::BeginTreeNode("Spotlight Shadow Map", false))
				{
					auto fb = m_Context->m_SpotShadowPass->GetTargetFramebuffer();
					auto image = fb->GetDepthImage();

					float size = ImGui::GetContentRegionAvail().x; // (float)fb->GetWidth() * 0.5f, (float)fb->GetHeight() * 0.5f
					if (m_Context->m_ResourcesCreated)
					{
						UI::Image(image, { size, size }, { 0, 1 }, { 1, 0 });
					}
					UI::EndTreeNode();
				}

				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);

#if 0
			if (UI::BeginTreeNode("Compute Bloom"))
			{
				float size = ImGui::GetContentRegionAvailWidth();
				if (m_ResourcesCreated)
				{
					static int tex = 0;
					UI::PropertySlider("Texture", tex, 0, 2);
					static int mip = 0;
					auto [mipWidth, mipHeight] = m_BloomComputeTextures[tex]->GetMipSize(mip);
					std::string label = fmt::format("Mip ({0}x{1})", mipWidth, mipHeight);
					UI::PropertySlider(label.c_str(), mip, 0, m_BloomComputeTextures[tex]->GetMipLevelCount() - 1);
					UI::ImageMip(m_BloomComputeTextures[tex]->GetImage(), mip, { size, size * (1.0f / m_BloomComputeTextures[tex]->GetImage()->GetAspectRatio()) }, { 0, 1 }, { 1, 0 });
				}
				UI::EndTreeNode();
			}
#endif

			if (UI::BeginTreeNode("Pipeline Resources", false))
			{
				if (m_Context->m_ResourcesCreated)
				{
					const float size = ImGui::GetContentRegionAvail().x;
					static int32_t mip = 0;
					UI::PropertySlider("HZB Mip", mip, 0, (int)m_Context->m_HierarchicalDepthTexture.Texture->GetMipLevelCount() - 1);
					UI::ImageMip(m_Context->m_HierarchicalDepthTexture.Texture->GetImage(), mip, { size, size * (0.9f / 1.6f) }, { 0, 1 }, { 1, 0 });

				}
				UI::EndTreeNode();
			}
			else
				UI::ShiftCursorY(headerSpacingOffset);
		}

		ImGui::End();
		return changed;
	}

}
