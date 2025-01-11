#pragma once
#include "BLAS.h"

namespace Beyond {

	class TLAS : public RefCounted
	{
	public:
		virtual bool IsReady() = 0;

		static Ref<TLAS> Create(bool motion, const eastl::string& name);
	};

}

