#pragma once

#if __has_include(<cxxabi.h>)
#define BEY_ABI_SUPPORTED
#include <cxxabi.h>
#endif

#include <type_traits>
#include <string>

#include "Beyond/Core/Hash.h"

namespace Beyond {

	// NOTE: StringType is used because on non-MSVC compilers we have to make a copy of the buffer we get back from abi::__cxa_demangle
	//				This is because that function allocates a heap buffer that we're responsible for freeing.
	//				But on Windows we can simply return a eastl::string_view since no heap allocation is necessary.
	//				We could of course handle non-MSVC compilers explicitly just like we do with MSVC, but it's not worth it imo. I may end up merging this with Jay's TypeDescriptor
	//				file if he thinks they fit together.

#if defined(BEY_ABI_SUPPORTED)
	using TypeNameString = eastl::string;

	template<typename T, bool ExcludeNamespace>
	struct TypeInfoBase
	{
	protected:
		TypeNameString DemangleTypeName(const eastl::string& typeName) const
		{
			size_t bufferLength = 0;
			int status = 0;
			char* buffer = abi::__cxa_demangle(typeName.c_str(), NULL, &bufferLength, &status);
			TypeNameString result = TypeNameString(buffer, bufferLength);
			free(buffer);

			if constexpr (ExcludeNamespace)
			{
				size_t namespacePos = result.find("::");

#ifndef BEY_PLATFORM_WINDOWS
				if (namespacePos != TypeNameString::npos)
					return result.substr(namespacePos + 2);
#endif
			}

			return result;
		}
	};
#else
	using TypeNameString = eastl::string_view;

	template<typename T, bool ExcludeNamespace>
	struct TypeInfoBase
	{
	protected:
		TypeNameString DemangleTypeName(eastl::string_view typeName) const
		{
			size_t spacePos = typeName.find(' ');
			if (spacePos != eastl::string_view::npos)
				typeName.remove_prefix(spacePos + 1);

			if constexpr (ExcludeNamespace)
			{
				size_t namespacePos = typeName.find("::");
				if (namespacePos != eastl::string_view::npos)
					typeName.remove_prefix(namespacePos + 2);
			}

			return typeName;
		}
	};

#endif

	template<typename T, bool ExcludeNamespace = false>
	struct TypeInfo : TypeInfoBase<T, ExcludeNamespace>
	{
	public:
		using Base = TypeInfoBase<T, ExcludeNamespace>;

	public:
		TypeInfo()
			: m_DemangledName(Base::DemangleTypeName(typeid(T).name()))
		{
		}

		TypeNameString Name() { return m_DemangledName; }
		const TypeNameString& Name() const { return m_DemangledName; }
		uint32_t HashCode() const { return Hash::GenerateFNVHash(m_DemangledName.data()); }

	private:
		TypeNameString m_DemangledName;
	};
}