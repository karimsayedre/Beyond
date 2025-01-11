#pragma once
#include "Material.h"

namespace Beyond {

	class ThreadedTextureLoader
	{
	public:
		void SubmitTexture(Ref<Material> material, const eastl::string& name, Ref<Texture2D> texture);
	};

}

