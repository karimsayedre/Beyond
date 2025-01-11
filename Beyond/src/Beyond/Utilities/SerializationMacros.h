#pragma once

#define BEY_SERIALIZE_PROPERTY(propName, propVal, outputNode) outputNode << YAML::Key << #propName << YAML::Value << propVal

#define BEY_SERIALIZE_PROPERTY_ASSET(propName, propVal, outputData) outputData << YAML::Key << #propName << YAML::Value << (propVal ? (uint64_t)propVal->Handle : 0);

#define BEY_DESERIALIZE_PROPERTY(propertyName, destination, node, defaultValue)	\
do {																			\
if (node.IsMap())																\
{																				\
	if (auto foundNode = node[#propertyName])									\
	{																			\
		try																		\
		{																		\
			destination = foundNode.as<decltype(defaultValue)>();				\
		}																		\
		catch (const std::exception& e)											\
		{																		\
			BEY_CONSOLE_LOG_ERROR(e.what());										\
			BEY_CORE_ERROR(e.what());											\
																				\
			destination = defaultValue;											\
		}																		\
	}																			\
	else																		\
	{																			\
		destination = defaultValue;												\
	}																			\
}																				\
else																			\
{																				\
	destination = defaultValue;													\
} } while(false)

#define BEY_DESERIALIZE_PROPERTY_RYML(propertyName, destination, node, defaultValue)				\
do{																								\
	if ((node).is_map())																		\
	{																							\
		if ((node).has_child(#propertyName))													\
		{																						\
			(node)[#propertyName] >> (destination);												\
		}																						\
		else																					\
		{																						\
			(destination) = defaultValue;														\
		}																						\
	}																							\
	else																						\
	{																							\
		(destination) = defaultValue;															\
	}																							\
}																								\
while (false)

#define BEY_DESERIALIZE_PROPERTY_ASSET(propName, destination, inputData, assetClass)\
	do																				\
		{AssetHandle assetHandle = inputData[#propName] ? inputData[#propName].as<uint64_t>() : 0;\
		if (AssetManager::IsAssetHandleValid(assetHandle))\
		{ destination = AssetManager::GetAsset<assetClass>(assetHandle); }\
		else\
		{ BEY_CORE_ERROR_TAG("AssetManager", "Tried to load invalid asset {0}.", #assetClass); }} \
		while(false)
