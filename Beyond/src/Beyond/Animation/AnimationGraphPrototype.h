#pragma once
#include "Beyond/Asset/Asset.h"
#include "Beyond/Core/Base.h"
#include "Beyond/Core/Identifier.h"
#include "Beyond/Core/UUID.h"

#include <choc/containers/choc_Value.h>


namespace Beyond::AnimationGraph {

	struct Prototype
	{
		std::string DebugName;
		UUID ID;

		AssetHandle Skeleton = 0;

		Prototype(AssetHandle skeleton) : Skeleton(skeleton) {}

		struct Endpoint
		{
			Identifier ID;
			choc::value::Value DefaultValue;

			Endpoint(Identifier id, choc::value::ValueView defaultValue) : ID(id), DefaultValue(defaultValue) {}

			Endpoint() : ID(0u), DefaultValue() {}
		};
		std::vector<Endpoint> Inputs;
		std::vector<Endpoint> Outputs;
		std::vector<Endpoint> LocalVariablePlugs;

		struct Node
		{
			Identifier TypeID; // Used to call Factory to create the right node type
			UUID ID;
			std::vector<Endpoint> DefaultValuePlugs;

			Scope<Prototype> Subroutine;

			Node(Identifier typeID, UUID id) : TypeID(typeID), ID(id) {}
			Node() : TypeID(0u), ID(0) {}
		};
		std::vector<Node> Nodes;

		struct Connection
		{
			enum EType
			{
				NodeValue_NodeValue = 0,
				NodeEvent_NodeEvent = 1,
				GraphValue_NodeValue = 2,
				GraphValue_GraphValue = 4, // In "transition" graphs, it is possible to connect an input variable directly to graph output. E.g a simple boolean input that activates the transition
				GraphEvent_NodeEvent = 5,
				NodeValue_GraphValue = 6,
				NodeEvent_GraphEvent = 7,
				GraphEvent_GraphEvent = 8,  // There's nothing stopping a user from connecting a graph input of type "trigger" directly to a graph output event.
				LocalVariable_NodeValue = 9,
				// LocalVariable_NodeEvent = 10,  needed?
			};

			struct Endpoint
			{
				UUID NodeID;
				Identifier EndpointID;
			};
			Endpoint Source;
			Endpoint Destination;

			EType Type;

			Connection(Endpoint&& source, Endpoint&& destination, EType connectionType) : Source(source), Destination(destination), Type(connectionType) {}
			Connection() : Source{ 0u, 0u }, Destination{ 0u, 0u }, Type(NodeValue_NodeValue) {}
		};
		std::vector<Connection> Connections;

	};

} // namespace Beyond::AnimationGraph
