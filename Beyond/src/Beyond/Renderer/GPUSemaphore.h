#pragma once

#include <Beyond/Core/Ref.h>

namespace Beyond
{
	
class GPUSemaphore : public RefCounted
{

public:
	static Ref<GPUSemaphore> Create(bool signaled = false);
};


}
