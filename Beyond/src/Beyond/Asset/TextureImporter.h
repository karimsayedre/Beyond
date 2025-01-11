#pragma once

#include "Beyond/Renderer/Texture.h"

#include <filesystem>

#include "Beyond/Core/Thread.h"

namespace Beyond {

	class TextureImporter
	{
	public:
		static std::vector<Buffer> CompressTexture(std::filesystem::path& path, TextureSpecification& spec, uint32_t mipLevels, const eastl::string& compressFormat);
		static std::vector<Buffer> ReadCompressedTexture(std::filesystem::path& path, TextureSpecification& spec);
		static std::vector<Buffer> ToBufferFromFile(std::filesystem::path& path, std::atomic_bool& found, TextureSpecification& spec);
		static Buffer ToBufferFromMemory(Buffer buffer, TextureSpecification& spec);
		static void WriteImageToFile(const std::filesystem::path& path, const ImageSpecification& spec, Buffer buffer);
	}; 
}
