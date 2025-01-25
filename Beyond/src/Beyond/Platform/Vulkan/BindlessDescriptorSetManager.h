#pragma once

#include "RenderPassInput.h"
#include "VulkanShader.h"


namespace Beyond {

	struct ResourceDesID
	{
		// frame	2  bits offset = 0
		// set		4  bits offset = 2
		// binding	26 bits offset = 6
		//constexpr ResourceDesID(const uint32_t shaderHash, const uint32_t frame, const uint32_t set, const uint32_t binding)
		//{
		//	BEY_CORE_ASSERT(frame <= 3);            // Ensure frame fits within 2 bits
		//	BEY_CORE_ASSERT(set <= 15);             // Ensure set fits within 4 bits
		//	BEY_CORE_ASSERT(binding <= 1000);	   // Ensure binding < 1000

		//	m_ID =  (shaderHash) << 32
		//		| (frame & 0b11) << 30            // Frame in the highest 2 bits
		//		| (set & 0b1111) << 26          // Set in the next 4 bits
		//		| (binding & 0x03FF'FFFF);      // Binding in the remaining 26 bits
		//}

		constexpr ResourceDesID(const uint32_t frame, const uint32_t set, const uint32_t binding)
		{
			BEY_CORE_ASSERT(frame <= 3);            // Ensure frame fits within 2 bits
			BEY_CORE_ASSERT(set <= 15);             // Ensure set fits within 4 bits
			BEY_CORE_ASSERT(binding <= 1000);	   // Ensure binding < 1000

			m_ID = (frame & 0b11) << 30            // Frame in the highest 2 bits
				| (set & 0b1111) << 26			   // Set in the next 4 bits
				| (binding & 0x03FF'FFFF);   // Binding in the remaining 26 bits

		}

		ResourceDesID() = default;

		ResourceDesID& operator=(const uint32_t that)
		{
			m_ID = that;
			return *this;
		}

		constexpr uint32_t GetFrame() const
		{
			return (m_ID >> 30) & 0b11;
		}
		constexpr uint32_t GetSet() const
		{
			return (m_ID >> 26) & 0xF;
		}
		constexpr uint32_t GetBinding() const
		{
			return m_ID & 0x03FF'FFFF;
		}

		constexpr void SetFrame(const uint32_t frame)
		{
			m_ID |= (frame & 0b11) << 30;
		}
		constexpr void SetSet(const uint32_t set)
		{
			m_ID &= (set & 0xFF) << 26;
		}
		constexpr void SetBinding(const uint32_t binding)
		{
			m_ID &= (binding & 0x03FF'FFFF);
		}

		bool operator<(const ResourceDesID& other) const
		{
			return m_ID < other.m_ID;
		}

		ResourceDesID GetSetBindingOnly() const
		{
			ResourceDesID result;
			result.m_ID = (m_ID) & 0x3FFF'FFFF;
			return result;
		}

	private:
		uint32_t m_ID;
	};

	template<typename ValueType>
	class ResourceDesMap
	{
		using KeyType = ResourceDesID;
		using MapType = std::map<KeyType, ValueType>;

		template <typename Iterator>
		class IteratorRange
		{
		public:
			IteratorRange(Iterator begin, Iterator end) : m_Begin(begin), m_End(end) {}

			Iterator begin() const { return m_Begin; }
			Iterator end() const { return m_End; }

			bool Empty() const { return m_Begin == m_End; }

		private:
			Iterator m_Begin;
			Iterator m_End;
		};

	public:
		size_t Size() { return m_Map.size(); }

		ValueType& operator[](const KeyType& key)
		{
			return m_Map[key];
		}

		bool Contains(uint32_t frame, uint32_t set) const
		{
			return std::ranges::any_of(m_Map | std::ranges::views::keys, [&](const KeyType key)
			{
				return key.GetFrame() == frame && key.GetSet() == set;
			});
		}

		bool Contains(const uint32_t frame, const uint32_t set, const uint32_t binding) const
		{
			ResourceDesID key(frame, set, binding);
			return m_Map.contains(key);
		}

		// Overload to check using ResourceDesID directly
		bool Contains(const ResourceDesID& key) const
		{
			return m_Map.contains(key);
		}

		// Optional: method to get the value if it exists
		const ValueType& Get(const uint32_t frame, const uint32_t set, const uint32_t binding) const
		{
			ResourceDesID key(frame, set, binding);
			return m_Map.at(key);
		}

		// Overload to get using ResourceDesID directly
		ValueType& Get(const ResourceDesID& key)
		{
			return m_Map.at(key);
		}

		// Overload to get using ResourceDesID directly
		const ValueType& Get(const ResourceDesID& key) const
		{
			return m_Map.at(key);
		}

		// Get a range for a specific frame
		auto GetRangeForFrame(uint32_t frame) const
		{
			KeyType frameStart(frame, 0, 0);
			KeyType frameEnd(frame + 1, 0, 0);
			return IteratorRange(m_Map.lower_bound(frameStart), m_Map.lower_bound(frameEnd));
		}

		void ClearFrameRange(uint32_t frame)
		{
			KeyType frameStart(frame, 0, 0);
			KeyType frameEnd(frame + 1, 0, 0);
			m_Map.erase(m_Map.lower_bound(frameStart), m_Map.lower_bound(frameEnd));
		}

		// Get a range for a specific set within a frame
		auto GetRangeForSet(uint32_t frame, uint32_t set) const
		{
			KeyType setStart(frame, set, 0);
			KeyType setEnd(frame, set + 1, 0);
			return IteratorRange(m_Map.lower_bound(setStart), m_Map.lower_bound(setEnd));
		}

		// Get a range for a specific frame
		auto GetRangeForFrame(const KeyType key) const
		{
			BEY_CORE_ASSERT(key.GetSet() == 0);
			BEY_CORE_ASSERT(key.GetBinding() == 0);
			KeyType frameEnd(key.GetFrame() + 1, 0, 0);
			return IteratorRange(m_Map.lower_bound(key), m_Map.lower_bound(frameEnd));
		}

		// Get a range for a specific set within a frame
		auto GetRangeForSet(const KeyType key) const
		{
			BEY_CORE_ASSERT(key.GetBinding() == 0);
			KeyType setEnd(key.GetFrame(), key.GetSet() + 1, 0);
			return IteratorRange(m_Map.lower_bound(key), m_Map.lower_bound(setEnd));
		}


		bool IsRangeEmpty(uint32_t frame, uint32_t set = UINT32_MAX, uint32_t binding = UINT32_MAX, uint32_t shaderHash = UINT32_MAX) const
		{
			if (set == UINT32_MAX)
			{
				// Check if the frame range is empty
				KeyType frameStart(frame, 0, 0);
				KeyType frameEnd(frame + 1, 0, 0);
				auto [begin, end] = std::make_pair(m_Map.lower_bound(frameStart), m_Map.lower_bound(frameEnd));
				return begin == end;
			}
			else if (binding == UINT32_MAX)
			{
				// Check if the set range within the frame is empty
				KeyType setStart(frame, set, 0);
				KeyType setEnd(frame, set + 1, 0);
				auto [begin, end] = std::make_pair(m_Map.lower_bound(setStart), m_Map.lower_bound(setEnd));
				return begin == end;
			}
			else
			{
				// Check if a specific binding exists
				KeyType id(frame, set, binding);
				return !m_Map.contains(id);
			}
		}

		void ClearEmptyResources(uint32_t frameIndex)
		{
			std::vector<ResourceDesID> resourcesToDelete;
			for (auto& [key, resource] : GetRangeForFrame(frameIndex))
			{
				if (resource.Type == RenderPassResourceType::None)
					resourcesToDelete.push_back(key);
			}
			for (ResourceDesID key : resourcesToDelete)
				m_Map.erase(key);
		}

		void Clear() { m_Map.clear(); }

	private:
		MapType m_Map;
	};




	struct BindlessDescriptorSetManagerSpecification
	{
		// Shaders that include the bindless set
		std::vector<Ref<VulkanShader>> Shaders;

		eastl::string DebugName;

		// Which descriptor sets should be managed
		uint32_t Set = (uint32_t)DescriptorSetAlias::Bindless;
		uint32_t DynamicSet = (uint32_t)DescriptorSetAlias::DynamicBindless;
		uint32_t MaxResources = 1000;

		bool DefaultResources = false;
	};

	struct BindlessDescriptorSetManager : public RefCounted
	{
		//
		// Input Resources (map of set->binding->resource)
		// 
		// Invalidated input resources will attempt to be assigned on Renderer::BeginRenderPass
		// This is useful for resources that may not exist at RenderPass creation but will be
		// present during actual rendering

		// frame = 0 -> set -> binding -> input
		ResourceDesMap<RenderPassInput> InputResources;

		// frame -> set -> binding -> input
		ResourceDesMap<std::set<RootSignature>> UpdatedInputResources;
		std::map<eastl::string, RenderPassInputDeclaration> InputDeclarations;

		// Key -> vector of shader hashes
		//std::unordered_map</*size_t*/, std::set<ResourceDesID>> UpdatedResourcesPerShader;

		// RootSignature -> set -> FrameInFlight -> VkDescriptorSet
		std::unordered_map<RootSignature, std::map<uint32_t, std::vector<VkDescriptorSet>>> m_DescriptorSets;

		struct WriteDescriptor
		{
			VkWriteDescriptorSet WriteDescriptorSet{};
			std::vector<const void*> ResourceHandles;
		};
		// frame -> set -> binding -> write descriptors
		ResourceDesMap<WriteDescriptor> WriteDescriptorMap;

		BindlessDescriptorSetManager(const BindlessDescriptorSetManager& other);
		BindlessDescriptorSetManager(BindlessDescriptorSetManagerSpecification specification);

		const RenderPassInputDeclaration* GetInputDeclaration(const eastl::string& name) const;
		// return true if already set
		bool SetBindlessInput(const RenderPassInput& input);

		void SetShader(Ref<VulkanShader> shaders);
		void BakeAll();
		void InvalidateAndUpdate();


		bool IsInvalidated(uint32_t frame, uint32_t set, uint32_t binding) const;
		bool Validate();
		void Release();

		//bool HasBufferSets() const;

		bool HasDescriptorSets(Ref<VulkanShader> shader) const;
		uint32_t GetFirstSetIndex(const RootSignature rootSignature) const;
		std::vector<VkDescriptorSet> GetDescriptorSets(RootSignature rootSignature, uint32_t frameIndex)const;
		void OnShaderReloaded();

		void AllocateDescriptorSets();
		std::vector<unsigned int> GetSets() { return { m_Specification.Set, m_Specification.DynamicSet }; }

	private:
		void Init();

	private:
		BindlessDescriptorSetManagerSpecification m_Specification;
		VkDescriptorPool m_DescriptorPool = nullptr;
	};

}
