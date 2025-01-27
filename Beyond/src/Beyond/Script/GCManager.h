#pragma once

#include "ScriptTypes.h"

extern "C" {
	typedef struct _MonoObject MonoObject;
}

namespace Beyond {

	using GCHandle = void*;

	class GCManager
	{
	public:
		static void Init();
		static void Shutdown();

		static void CollectGarbage(bool blockUntilFinalized = true);

		static GCHandle CreateObjectReference(MonoObject* managedObject, bool weakReference, bool pinned = false, bool track = true);
		static bool IsHandleValid(GCHandle handle);
		static MonoObject* GetReferencedObject(GCHandle handle);
		static void ReleaseObjectReference(GCHandle handle);
	};

}
