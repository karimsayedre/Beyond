#include "pch.h"
#include "SoundGraphPrototype.h"

namespace Beyond::SoundGraph
{
	void Prototype::Endpoint::Serialize(StreamWriter* writer, const Endpoint& endpoint)
	{
		writer->WriteRaw((uint32_t)endpoint.EndpointID);

		Serialization::ValueSerializer wrapper;
		endpoint.DefaultValue.serialise(wrapper);
		writer->WriteArray(wrapper.Data);
	}

	void Prototype::Endpoint::Deserialize(StreamReader* reader, Endpoint& endpoint)
	{
		uint32_t id;
		reader->ReadRaw(id);
		endpoint.EndpointID = id;

		std::vector<uint8_t> data;
		reader->ReadArray(data);
        choc::value::InputData input_data{ data.data(), data.data() + data.size() };
		endpoint.DefaultValue = choc::value::Value::deserialise(input_data);
	}

	void Prototype::Serialize(StreamWriter* writer, const Prototype& prototype)
	{
		BEY_CORE_ASSERT(false);

		//Serialization::Serialize(writer, prototype);
	}

	void Prototype::Deserialize(StreamReader* reader, Prototype& prototype)
	{
		BEY_CORE_ASSERT(false);

		//Serialization::Deserialize(reader, prototype);
	}

	void Prototype::Connection::Serialize(StreamWriter* writer, const Connection& endpoint)
	{
		BEY_CORE_ASSERT(false);
	}

	void Prototype::Connection::Deserialize(StreamReader* reader, Connection& endpoint)
	{
		BEY_CORE_ASSERT(false);

	}

	void Prototype::Node::Serialize(StreamWriter* writer, const Node& endpoint)
	{
		BEY_CORE_ASSERT(false);

	}

	void Prototype::Node::Deserialize(StreamReader* reader, Node& endpoint)
	{
		BEY_CORE_ASSERT(false);

	}

} // namespace Beyond::SoundGraph
