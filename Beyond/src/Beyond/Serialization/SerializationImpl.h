#pragma once

#include "Beyond/Serialization/StreamWriter.h"
#include "Beyond/Serialization/StreamReader.h"

#include "Beyond/Reflection/TypeDescriptor.h"
#include "Beyond/Core/Identifier.h"
#include "Beyond/Core/UUID.h"


//=============================================================================
/**  Non-intrusive Binary / Memory Serialization Interface.

	Add SERIALIZABLE macro in a header for class you need to be serialized
	member by member. For example:

	SERIALIZABLE(MyClass,
				&MyClass::MemberPrimitive,
				&MyClass::MemberVector,
				&MyClass::MemberAnotherSerializable);

	If ther's no spatialization for the ..Impl (de)serialize funcion for any
	of the listed in the macro members, it's going to call:
	
		T::Serialize(StreamWriter* writer, T& obj)
	..or
		T::Deserialize(StreamReader* reader, T& obj)

	..where T is the type of the object that need to have Serialzie and Deserialize
	functions with this signature.

	Instead of adding intrusive interface as described above you can add to your
	header specialization for the free template functions as follows:

	namespace Beyond::Spatialization::Impl
	{
		template<>
		static inline bool SerializeImpl(StreamWriter* writer, const MyClass& v)
		{
			... some custom serialization routine ...
			writer->WriteRaw(42069);
			return true;
		}

		template<>
		static inline bool DeserializeImpl(StreamReader* reader, MyClass& v)
		{
			... some custom deserialization routine ...
			reader->ReadRaw(v.MyVariable);
			return true;
		}
	} // namespace Beyond::Spatialization::Impl
*/
#define SERIALIZABLE(Class, ...) template<>\
struct Beyond::Type::Description<Class> : Beyond::Type::MemberList<__VA_ARGS__> {};\

namespace Beyond::Serialization
{
	//=============================================================================
	/// Serialization
	template<typename... Ts>
	static bool Serialize(StreamWriter* writer, const Ts&...vs);

	namespace Impl
	{
		template<typename T>
		static bool SerializeImpl(StreamWriter* writer, const T& v)
		{
			if constexpr (std::is_trivial<T>::value)
				writer->WriteRaw(v);
			else
				T::Serialize(writer, v);

			return true;
		}
		
		//=============================================================================
		/** Serialization specialization
			More default specializations can be added...
		*/

		template<>
		BEY_EXPLICIT_STATIC inline bool SerializeImpl(StreamWriter* writer, const UUID& v)
		{
			writer->WriteRaw((uint64_t)v);
			return true;
		}

		template<>
		BEY_EXPLICIT_STATIC inline bool SerializeImpl(StreamWriter* writer, const Identifier& v)
		{
			writer->WriteRaw((uint32_t)v);
			return true;
		}

		template<>
		BEY_EXPLICIT_STATIC inline bool SerializeImpl(StreamWriter* writer, const eastl::string& v)
		{
			writer->WriteString(v);
			return true;
		}
		
		//=============================================================================
		template<typename T>
		BEY_EXPLICIT_STATIC bool SerializeVec(StreamWriter* writer, const std::vector<T>& vec)
		{
			//if (writeSize)
			writer->WriteRaw<uint32_t>((uint32_t)vec.size());

			for (const auto& element : vec)
				Serialize(writer, element);

			return true;
		}

		//=============================================================================
		template<typename T>
		BEY_EXPLICIT_STATIC bool SerializeByType(StreamWriter* writer, const T& v)
		{
			if constexpr (Type::Described<T>::value)
			{
				return Type::Description<T>::Apply(
						[&writer](const auto&... members)
				{
					return Serialize(writer, members...);
				}
				, v);
			}
			else if constexpr (Type::is_array_v<T>)
			{
				return SerializeVec(writer, v);
			}
			else
			{
				return SerializeImpl(writer, v);
			}
		}
	} // namespace Impl

	template<typename... Ts>
	static bool Serialize(StreamWriter* writer, const Ts&...vs)
	{
		return (Impl::SerializeByType(writer, vs) && ...);
	}


	//=============================================================================
	/// Deserialization
	template<typename... Ts>
	static bool Deserialize(StreamReader* reader, Ts&...vs);

	namespace Impl
	{
		template<typename T>
		static bool DeserializeImpl(StreamReader * reader, T & v)
		{
			if constexpr (std::is_trivial<T>::value)
				reader->ReadRaw(v);
			else
				T::Deserialize(reader, v);

			return true;
		}

		//=============================================================================
		/** Deserialization specialization
			More default specializations can be added...
		*/

		template<>
		BEY_EXPLICIT_STATIC inline bool DeserializeImpl(StreamReader * reader, UUID & v)
		{
			reader->ReadRaw((uint64_t&)v);
			return true;
		}

		template<>
		BEY_EXPLICIT_STATIC inline bool DeserializeImpl(StreamReader * reader, Identifier & v)
		{
			uint32_t data;
			reader->ReadRaw(data);
			v = data;
			return true;
		}

		template<>
		BEY_EXPLICIT_STATIC inline bool DeserializeImpl(StreamReader * reader, eastl::string & v)
		{
			reader->ReadString(v);
			return true;
		}

		//=============================================================================
		template<typename T>
		BEY_EXPLICIT_STATIC bool DeserializeVec(StreamReader * reader, std::vector<T>&vec)
		{
			uint32_t size = 0;
			//if (size == 0)
			reader->ReadRaw<uint32_t>(size);

			vec.resize(size);

			for (uint32_t i = 0; i < size; i++)
				Deserialize(reader, vec[i]);

			return true;
		}

		//=============================================================================
		template<typename T>
		BEY_EXPLICIT_STATIC bool DeserializeByType(StreamReader * reader, T & v)
		{
			if constexpr (Type::Described<T>::value)
			{
				return Type::Description<T>::Apply(
						[&reader](auto&... members)
				{
					return Deserialize(reader, members...);
				}
				, v);
			}
			else if constexpr (Type::is_array_v<T>)
			{
				return DeserializeVec(reader, v);
			}
			else
			{
				return DeserializeImpl(reader, v);
			}
		}
	} // namespace Impl

	template<typename... Ts>
	static bool Deserialize(StreamReader* reader, Ts&... vs)
	{
		return (Impl::DeserializeByType(reader, vs) && ...);
	}

} // namespace Beyond::Serialization
