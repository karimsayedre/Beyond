#pragma once

namespace Beyond {

	struct PhysicsLayer
	{
		uint32_t LayerID;
		eastl::string Name;
		int32_t BitValue;
		int32_t CollidesWith = 0;
		bool CollidesWithSelf = true;

		bool IsValid() const
		{
			return !Name.empty() && BitValue > 0;
		}
	};

	class PhysicsLayerManager
	{
	public:
		static uint32_t AddLayer(const eastl::string& name, bool setCollisions = true);
		static void RemoveLayer(uint32_t layerId);

		static void UpdateLayerName(uint32_t layerId, const eastl::string& newName);

		static void SetLayerCollision(uint32_t layerId, uint32_t otherLayer, bool shouldCollide);
		static std::vector<PhysicsLayer> GetLayerCollisions(uint32_t layerId);

		static const std::vector<PhysicsLayer>& GetLayers() { return s_Layers; }
		static const std::vector<eastl::string>& GetLayerNames() { return s_LayerNames; }

		static PhysicsLayer& GetLayer(uint32_t layerId);
		static PhysicsLayer& GetLayer(const eastl::string& layerName);
		static uint32_t GetLayerCount() { return uint32_t(s_Layers.size()); }

		static bool ShouldCollide(uint32_t layer1, uint32_t layer2);
		static bool IsLayerValid(uint32_t layerId);
		static bool IsLayerValid(const eastl::string& layerName);

		static void ClearLayers();

	private:
		static uint32_t GetNextLayerID();

	private:
		static std::vector<PhysicsLayer> s_Layers;
		static std::vector<eastl::string> s_LayerNames;
		static PhysicsLayer s_NullLayer;
	};

}
