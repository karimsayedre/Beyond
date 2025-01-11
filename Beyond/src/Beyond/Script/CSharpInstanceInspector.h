#pragma once

#include "Beyond/Core/Buffer.h"

extern "C" {
	typedef struct _MonoObject MonoObject;
}

namespace Beyond {

	class CSharpInstanceInspector
	{
	public:
		CSharpInstanceInspector(MonoObject* instance);

		bool HasName(std::string_view fullName) const;
		bool InheritsFrom(std::string_view fullName) const;

		const char* GetName() const;

		template<typename T>
		T GetFieldValue(std::string_view fieldName) const
		{
			Buffer valueBuffer = GetFieldBuffer(fieldName);

			if (!valueBuffer)
				return T();

			T value = T();
			memcpy(&value, valueBuffer.Data, sizeof(T));
			valueBuffer.Release();
			return value;
		}

	private:
		Buffer GetFieldBuffer(std::string_view fieldName) const;

	private:
		MonoObject* m_Instance;
	};

	template<>
	inline std::string CSharpInstanceInspector::GetFieldValue(std::string_view fieldName) const
	{
		Buffer valueBuffer = GetFieldBuffer(fieldName);

		if (!valueBuffer)
			return std::string();

		std::string value((char*)valueBuffer.Data, valueBuffer.Size / sizeof(char));
		valueBuffer.Release();
		return value;
	}


}
