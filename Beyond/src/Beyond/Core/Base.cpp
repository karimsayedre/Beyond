#include "pch.h"
#include "Base.h"

#include "Log.h"
#include "Memory.h"

#define BEY_BUILD_ID "v0.1a"

namespace Beyond {

	void InitializeCore()
	{
		Allocator::Init();
		Log::Init();

		BEY_CORE_TRACE_TAG("Core", "Beyond Engine {}", BEY_BUILD_ID);
		BEY_CORE_TRACE_TAG("Core", "Initializing...");
	}

	void ShutdownCore()
	{
		BEY_CORE_TRACE_TAG("Core", "Shutting down...");
		
		Log::Shutdown();
	}

}
