#pragma once

#include <map>
#include <mutex>

namespace Beyond {



	struct AllocationStats
	{
		size_t TotalAllocated = 0;
		size_t TotalFreed = 0;
	};

	struct Allocation
	{
		void* Memory = 0;
		size_t Size = 0;
		const char* Category = 0;
	};

	namespace Memory {
		const AllocationStats& GetAllocationStats();
	}

	template <class T>
	struct Mallocator
	{
		typedef T value_type;

		Mallocator() = default;
		template <class U> constexpr Mallocator(const Mallocator <U>&) noexcept {}

		T* allocate(std::size_t n)
		{
			if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
				throw std::bad_array_new_length();

			if (auto p = static_cast<T*>(std::malloc(n * sizeof(T))))
			{
				return p;
			}

			throw std::bad_alloc();
		}

		void deallocate(T* p, std::size_t n) noexcept
		{
			std::free(p);
		}
	};

	struct AllocatorData
	{
		using MapAlloc = Mallocator<std::pair<const void* const, Allocation>>;
		using StatsMapAlloc = Mallocator<std::pair<const char* const, AllocationStats>>;

		using AllocationStatsMap = std::map<const char*, AllocationStats, std::less<const char*>, StatsMapAlloc>;

		std::map<const void*, Allocation, std::less<>, MapAlloc> m_AllocationMap;
		AllocationStatsMap m_AllocationStatsMap;

		std::mutex m_Mutex, m_StatsMutex;
	};


	class Allocator
	{
	public:
		static void Init();

		static void* AllocateRaw(size_t size);

		static void* Allocate(size_t size);
		static void* Allocate(size_t size, const char* desc);
		static void* Allocate(size_t size, const char* file, int line);
		static void Free(void* memory);

		static const AllocatorData::AllocationStatsMap& GetAllocationStats() { return s_Data->m_AllocationStatsMap; }
	private:
		inline static AllocatorData* s_Data = nullptr;
	};


	struct EastlAllocator
	{
		EastlAllocator() = default;
		EastlAllocator(const char* name)
			: Name(name)
		{

		}

		static void* allocate(size_t size, int /*flags*/ = 0)
		{
			return Allocator::Allocate(size);
		}

		static void deallocate(void* memory, size_t /*size*/ = 0)
		{
			Allocator::Free(memory);
		}

		static void* allocate(size_t size, char const* file, int line, unsigned int type, char const* function, int blockType)
		{
			return Allocator::Allocate(size, file);
		}

		static void* allocate(unsigned __int64 size, unsigned __int64 alignment, unsigned __int64 offset, char const* file, int line, unsigned int type, char const* function, int blockType)
		{
			return Allocator::Allocate(size, file);
		}

		static void* allocate(unsigned __int64 size, unsigned __int64 alignment, unsigned __int64 offset = 0, int flags = 0)
		{
			return Allocator::Allocate(size);
		}

		// Define equality operator
		bool operator==(const EastlAllocator&) const noexcept
		{
			// Since EASTL allocators are typically stateless, they are often considered equal
			return true;
		}

		// Define inequality operator
		bool operator!=(const EastlAllocator&) const noexcept
		{
			return false;
		}


		// Optional: Track debug information
		const char* get_name() const { return Name; }
		void set_name(const char* name) { Name = name; }

		const char* Name = "MyEASTLAllocator";
	};


}

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __cdecl operator new[](size_t size, char const* file, int line, unsigned int type, char const* function, int blockType); // For EASTL

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __cdecl operator new[](unsigned __int64 size, unsigned __int64 alignment, unsigned __int64 offset, char const* file, int line, unsigned int type, char const* function, int blockType); // for EASTL

#undef BEY_TRACK_MEMORY
#if BEY_TRACK_MEMORY

#ifdef BEY_PLATFORM_WINDOWS

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* desc);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new(size_t size, const char* file, int line);

_NODISCARD _Ret_notnull_ _Post_writable_byte_size_(size) _VCRT_ALLOCATOR
void* __CRTDECL operator new[](size_t size, const char* file, int line);



void __CRTDECL operator delete(void* memory);
void __CRTDECL operator delete(void* memory, const char* desc);
void __CRTDECL operator delete(void* memory, const char* file, int line);
void __CRTDECL operator delete[](void* memory);
void __CRTDECL operator delete[](void* memory, const char* desc);
void __CRTDECL operator delete[](void* memory, const char* file, int line);

#define hnew new(__FILE__, __LINE__)
#define hdelete delete

#else
#warning "Memory tracking not available on non-Windows platform"
#define hnew new
#define hdelete delete

#endif

#else

#define hnew new
#define hdelete delete

#endif
