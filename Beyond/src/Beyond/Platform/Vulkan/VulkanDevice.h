#pragma once

#include <nvsdk_ngx_defs.h>

#include "Beyond/Core/Ref.h"

#include "Vulkan.h"

#include <unordered_set>
#include <Beyond/Platform/Vulkan/Debug/NsightAftermathGpuCrashTracker.h>



namespace Beyond {
	
	class VulkanPhysicalDevice : public RefCounted
	{
	public:
		struct QueueFamilyIndices
		{
			int32_t Graphics = -1;
			int32_t Compute = -1;
			int32_t Transfer = -1;
		};
	public:
		VulkanPhysicalDevice();
		~VulkanPhysicalDevice();

		bool IsExtensionSupported(const eastl::string& extensionName) const;
		uint32_t GetMemoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags properties) const;

		VkPhysicalDevice GetVulkanPhysicalDevice() const { return m_PhysicalDevice; }
		const QueueFamilyIndices& GetQueueFamilyIndices() const { return m_QueueFamilyIndices; }
		const VkPhysicalDeviceVulkan12Properties& GetProperties12() const { return m_Properties12; }
		const VkPhysicalDeviceProperties& GetProperties() const { return m_Properties; }
		const VkPhysicalDeviceProperties2& GetProperties2() const { return m_Properties2; }
		const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& GetRaytracingPipelineProperties() const { return m_RayTracingPipelineProperties; }
		const VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV& GetRaytracingInvocationReorderProperties() const { return m_RayTracingInvocationReorderProperties; }
		const VkPhysicalDeviceLimits& GetLimits() const { return m_Properties.limits; }
		const VkPhysicalDeviceAccelerationStructurePropertiesKHR& GetASProps() const { return m_AccelProperties; }
		const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_MemoryProperties; }

		VkFormat GetDepthFormat() const { return m_DepthFormat; }

		static Ref<VulkanPhysicalDevice> Select();
	private:
		VkFormat FindDepthFormat() const;
		QueueFamilyIndices GetQueueFamilyIndices(int queueFlags);
	private:
		QueueFamilyIndices m_QueueFamilyIndices;

		VkPhysicalDevice m_PhysicalDevice = nullptr;
		VkPhysicalDeviceProperties m_Properties;
		VkPhysicalDeviceVulkan12Properties m_Properties12;
		VkPhysicalDeviceFeatures m_Features;
		VkPhysicalDeviceMemoryProperties m_MemoryProperties;
		VkPhysicalDeviceAccelerationStructurePropertiesKHR m_AccelProperties{};
		VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_RayTracingPipelineProperties{};
		VkPhysicalDeviceRayTracingInvocationReorderPropertiesNV m_RayTracingInvocationReorderProperties{};

		VkPhysicalDeviceProperties2KHR m_Properties2{};

		VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

		std::vector<VkQueueFamilyProperties> m_QueueFamilyProperties;
		eastl::unordered_set<eastl::string> m_SupportedExtensions;
		std::vector<VkDeviceQueueCreateInfo> m_QueueCreateInfos;

		bool m_RaytracingSupported = false; // TODO: Actually use this
		bool m_BindlessSupported = false; // TODO: Actually use this

		friend class VulkanDevice;
	};

	class VulkanCommandPool : public RefCounted
	{
	public:
		VulkanCommandPool();
		virtual ~VulkanCommandPool();

		VkCommandBuffer AllocateCommandBuffer(const eastl::string& name, bool begin, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);

		VkCommandPool GetGraphicsCommandPool() const { return m_GraphicsCommandPool; }
		VkCommandPool GetComputeCommandPool() const { return m_ComputeCommandPool; }
	private:
		VkCommandPool m_GraphicsCommandPool, m_ComputeCommandPool;
	};

	// Represents a logical device
	class VulkanDevice : public RefCounted
	{
	public:
		VulkanDevice(const Ref<VulkanPhysicalDevice>& physicalDevice, VkPhysicalDeviceFeatures enabledFeatures);
		~VulkanDevice();

		void Destroy();

		VkQueue GetGraphicsQueue() { return m_GraphicsQueue; }
		const VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
		VkQueue GetComputeQueue() { return m_ComputeQueue; }

		VkCommandBuffer CreateCommandBuffer(const eastl::string& name, bool begin, bool compute = false);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer);
		void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue);

		VkCommandBuffer CreateSecondaryCommandBuffer(const char* debugName);

		const Ref<VulkanPhysicalDevice>& GetPhysicalDevice() const { return m_PhysicalDevice; }
		VkDevice GetVulkanDevice() const { return m_LogicalDevice; }
		const NVSDK_NGX_FeatureDiscoveryInfo& GetNgxFeatureDiscoveryInfo() const { return m_NgxFeatureDiscoveryInfo; }

		bool IsRaytracingSupported() { return m_AccelerationStructuresSupported; }
		bool IsDLSSSupported() { return m_DLSSSupported; }

#ifndef BEY_DIST
		inline static const GpuCrashTracker& GetGPUCrashTracker() { return s_GPUCrashTracker; }
#endif

	private:
		Ref<VulkanCommandPool> GetThreadLocalCommandPool();
		Ref<VulkanCommandPool> GetOrCreateThreadLocalCommandPool();
	private:
		VkDevice m_LogicalDevice = nullptr;
		Ref<VulkanPhysicalDevice> m_PhysicalDevice;
		VkPhysicalDeviceFeatures m_EnabledFeatures;

		VkQueue m_GraphicsQueue;
		VkQueue m_ComputeQueue;

		std::map<std::thread::id, Ref<VulkanCommandPool>> m_CommandPools;
		NVSDK_NGX_FeatureDiscoveryInfo m_NgxFeatureDiscoveryInfo;

		bool m_EnableDebugMarkers = false;
		bool m_DLSSSupported = false;
		bool m_RayQuerySupported = false;
		bool m_AccelerationStructuresSupported = false;
		bool m_RaytracingPipelineSupported = false;
		bool m_BindlessSupported = false;

		//inline static GpuCrashTracker::MarkerMap s_GPUCrashMarkerMap;
#ifndef BEY_DIST
		inline static GpuCrashTracker s_GPUCrashTracker;
#endif
	};


}
