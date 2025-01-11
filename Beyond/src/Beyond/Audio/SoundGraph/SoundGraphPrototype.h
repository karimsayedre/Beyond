#pragma once
#include "Beyond/Core/UUID.h"
#include "Beyond/Core/Identifier.h"
#include "containers/choc_Value.h"

#include "Beyond/Serialization/SerializationImpl.h"

namespace Beyond
{
	class StreamWriter;
	class StreamReader;
}

namespace Beyond::SoundGraph
{
	//==============================================================================
	/// SoundGraph Prototype, used to construct instances of SoundGraph for playback
	struct Prototype : public RefCounted
	{
		std::string DebugName;
		UUID ID;

		struct Endpoint
		{
			Identifier EndpointID;
			choc::value::Value DefaultValue;

			Endpoint(Identifier ID, choc::value::ValueView defaultValue)
				: EndpointID(ID), DefaultValue(defaultValue) {}

			Endpoint()
				: EndpointID(0u), DefaultValue() {}

			static void Serialize(Beyond::StreamWriter* writer, const Endpoint& endpoint);
			static void Deserialize(Beyond::StreamReader* reader, Endpoint& endpoint);
		};

		//==============================================================================
		/// Graph IO
		std::vector<Endpoint> Inputs, Outputs;

		// TODO: this should be removed and output channels should have hardcoded ids
		std::vector<Identifier> OutputChannelIDs;

		std::vector<Endpoint> LocalVariablePlugs;

		//==============================================================================
		/// Nodes
		struct Node
		{
			Identifier NodeTypeID; // Used to call Factory to create the right node type
			UUID ID;
			std::vector<Endpoint> DefaultValuePlugs;

			Node(Identifier typeID, UUID uniqueID) : NodeTypeID(typeID), ID(uniqueID) {}
			Node() : NodeTypeID(0u), ID(0) {}

			static void Serialize(Beyond::StreamWriter* writer, const Node& endpoint);
			static void Deserialize(Beyond::StreamReader* reader, Node& endpoint);
		};

		std::vector<Node> Nodes;

		//==============================================================================
		/// Connections
		struct Connection
		{
			enum EType
			{
				NodeValue_NodeValue = 0,
				NodeEvent_NodeEvent = 1,
				GraphValue_NodeValue = 2,
				GraphEvent_NodeEvent = 3,
				NodeValue_GraphValue = 4,
				NodeEvent_GraphEvent = 5,
				LocalVariable_NodeValue = 6,
			};

			struct Endpoint
			{
				UUID NodeID;
				Identifier EndpointID;
			};

			Endpoint Source, Destination;
			EType Type;

			Connection(Endpoint&& source, Endpoint&& destination, EType connectionType)
				: Source(source), Destination(destination), Type(connectionType)
			{}

			Connection()
				: Source{ 0u, 0u }, Destination{ 0u, 0u }, Type(NodeValue_NodeValue) {}

			static void Serialize(Beyond::StreamWriter* writer, const Connection& endpoint);
			static void Deserialize(Beyond::StreamReader* reader, Connection& endpoint);
		};

		// Used to create a copy of the graph
		std::vector<Connection> Connections;

		static void Serialize(Beyond::StreamWriter* writer, const Prototype& prototype);
		static void Deserialize(Beyond::StreamReader* reader, Prototype& prototype);
	};

} // namspace Beyond::SoundGraph

using Prototype = Beyond::SoundGraph::Prototype;

SERIALIZABLE(Prototype,
	&Prototype::DebugName,
	&Prototype::ID,
	&Prototype::Inputs,
	&Prototype::Outputs,
	&Prototype::OutputChannelIDs,
	&Prototype::LocalVariablePlugs,
	&Prototype::Nodes,
	&Prototype::Connections);

SERIALIZABLE(Prototype::Node,
	&Prototype::Node::NodeTypeID,
	&Prototype::Node::ID,
	&Prototype::Node::DefaultValuePlugs);

namespace Beyond::Serialization
{
	using PEndpoint = Prototype::Endpoint;

	struct ValueSerializer
	{
		std::vector<uint8_t> Data;
		void write(const void* data, size_t size)
		{
			Data.insert(Data.end(), (const uint8_t*)data, (const uint8_t*)data + size);
		}
	};
#if 1
	namespace Impl
	{
		//=============================================================================
		// Serialization
		template<>
		BEY_EXPLICIT_STATIC inline bool SerializeImpl(Beyond::StreamWriter* writer, const choc::value::Value& v)
		{
			struct ValueSerializer wrapper;
			v.serialise(wrapper);

			writer->WriteArray(wrapper.Data);
			return true;
		}


		template<>
		BEY_EXPLICIT_STATIC inline bool SerializeImpl(Beyond::StreamWriter* writer, const Prototype::Connection& v)
		{
			writer->WriteRaw(v);
			return true;
		}

		//=============================================================================
		// Deserialization

		template<>
		BEY_EXPLICIT_STATIC inline bool DeserializeImpl(Beyond::StreamReader* reader, choc::value::Value& v)
		{
			std::vector<uint8_t> data;
			reader->ReadArray(data);
			choc::value::InputData inputData{ data.data(), data.data() + data.size() };
			v = choc::value::Value::deserialise(inputData);
			return true;
		}

		template<>
		BEY_EXPLICIT_STATIC inline bool DeserializeImpl(Beyond::StreamReader* reader, Prototype::Connection& v)
		{
			reader->ReadRaw(v);
			return true;
		}
	} // namespace Impl
#endif
} // namespace Beyond::Type

