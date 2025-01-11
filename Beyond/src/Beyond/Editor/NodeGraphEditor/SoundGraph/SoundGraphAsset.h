#pragma once

#include "Beyond/Asset/Asset.h"

#include "Beyond/Audio/SoundGraph/SoundGraphPrototype.h"

#include "Beyond/Editor/NodeGraphEditor/Nodes.h"
#include "Beyond/Editor/NodeGraphEditor/PropertySet.h"

#include <filesystem>

namespace Beyond
{
	// TODO: technically only the Prototype is needed for the runtime from SoundGraphAsset
	class SoundGraphAsset : public Asset
	{
	public:
		std::vector<Node*> Nodes;
		std::vector<Link> Links;
		std::string GraphState; //? Should not have this in Runtime asset

		Utils::PropertySet GraphInputs;
		Utils::PropertySet GraphOutputs;
		Utils::PropertySet LocalVariables;

		Ref<SoundGraph::Prototype> Prototype; // TODO: add serialization / caching
		std::filesystem::path CachedPrototype;

		std::vector<UUID> WaveSources;

		SoundGraphAsset() = default;
		virtual ~SoundGraphAsset()
		{
			for (auto* node : Nodes)
				delete node;
		}

		static AssetType GetStaticType() { return AssetType::SoundGraphSound; }
		virtual AssetType GetAssetType() const override { return GetStaticType(); }
	};

} // namespace Beyond
