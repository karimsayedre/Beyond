#ifndef EASTL_CONFIG_H
#define EASTL_CONFIG_H

#include <Beyond/Core/Memory.h>

// Example: Define your allocator
#define EASTLAllocatorType Beyond::EastlAllocator
#define EASTLAllocatorDefault() get_default_allocator(static_cast<Beyond::EastlAllocator*>(nullptr))


namespace eastl {



}


//#define EASTLAllocatorDefault Beyond::Allocator*

//#define EASTL_USER_DEFINED_ALLOCATOR 1
// Enable debugging for EASTL
//#define EASTL_DEBUG BEY_DEBUG

// Set other configurations as needed
//#define EASTL_ASSERT(expr) BEY_CORE_ASSERT(expr)
//#define EASTL_CUSTOM_ASSERT_ENABLED 1

// Disable exceptions if your project doesn't use them
#define EASTL_EXCEPTIONS_ENABLED 0

#endif // EASTL_CONFIG_H
