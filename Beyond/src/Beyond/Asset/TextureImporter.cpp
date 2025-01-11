#include "pch.h"
#include "TextureImporter.h"

#include <stb_image_write.h>

#include <stb_image.h>
#include <compressonator.h>
//#include <nvtt/nvtt.h>
#include <tiny_dds/tinydds.h>

#include "CompressonatorHelpers/Cmips.h"
#include "Beyond/Core/Thread.h"
#include "Beyond/Core/Timer.h"
#include "Beyond/Platform/Vulkan/VulkanShaderUtils.h"
#include "Beyond/Utilities/ProcessHelper.h"

namespace Beyond {
	//---------------------------------------------------------------------------
// Sample loop back code called for each compression block been processed
//---------------------------------------------------------------------------
	/*bool CompressionCallback(float fProgress, CMP_DWORD_PTR pUser1, CMP_DWORD_PTR pUser2)
	{
		BEY_CORE_INFO("Compression progress = {}", fProgress);
		return false;
	}*/

	uint32_t PackB10G11R11UFLOAT(const float* data)
	{

		// Extract individual channels (assuming little-endian byte order)
		uint32_t blue = static_cast<uint32_t>(data[0] * 1023.0f); // 10 bits for blue
		uint32_t green = static_cast<uint32_t>(data[1] * 2047.0f); // 11 bits for green
		uint32_t red = static_cast<uint32_t>(data[2] * 2047.0f); // 11 bits for red

		// Pack the channels into a 32-bit value
		uint32_t packedValue = (blue << 0) | (green << 10) | (red << 21);

		return packedValue;
	}

	bool IsCompressedFormat(const ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::BC1_RGBA_SRGB:
			case ImageFormat::BC1_RGB_SRGB:
			case ImageFormat::BC1_RGBA_UNORM:
			case ImageFormat::BC2_SRGB:
			case ImageFormat::BC2_UNORM:
			case ImageFormat::BC3_SRGB:
			case ImageFormat::BC3_UNORM:
			case ImageFormat::BC4_SNORM:
			case ImageFormat::BC4_UNORM:
			case ImageFormat::BC5_SNORM:
			case ImageFormat::BC5_UNORM:
			case ImageFormat::BC6H_SFLOAT:
			case ImageFormat::BC6H_UFLOAT:
			case ImageFormat::BC7_SRGB:
			case ImageFormat::BC7_UNORM:
				return true;
			default: return false;
		}
	}

	ImageFormat GetImageFormat(int channels, bool isSRGB, bool isHDR, bool is16Bit)
	{
		if (isHDR)
		{
			if (channels == 3)
				return ImageFormat::RGB32F;
			else if (channels == 4)
				return ImageFormat::RGBA32F;
		}
		else if (is16Bit)
		{
			if (channels == 3)
				return ImageFormat::RGB16F;
			else if (channels == 4)
				return ImageFormat::RGBA16F;
		}
		else
		{
			switch (channels)
			{
				case 1:
					return ImageFormat::RED8UN;
				case 2:
					return ImageFormat::RG8;
				case 3: // unsupported
				case 4:
					return isSRGB ? ImageFormat::SRGBA : ImageFormat::RGBA;
					// Handle other channel counts if necessary
				default:
					// Default to RGBA
					return isSRGB ? ImageFormat::SRGBA : ImageFormat::RGBA;
			}
		}
		// Default fallback
		return ImageFormat::RGBA;
	}

	bool QuickDDSHasTransparency(const char* filename)
	{
		FILE* file = fopen(filename, "rb");

		if (!file) return false;

		// Read DDS header
		char header[128];
		fread(header, 1, 128, file);
		if (!file || std::memcmp(header, "DDS ", 4) != 0) return false;

		// Check pixel format
		uint32_t pixelFormatFlags = *reinterpret_cast<uint32_t*>(header + 80);
		if (pixelFormatFlags & 0x1)
		{  // DDPF_ALPHAPIXELS flag
			return true;
		}

		// Check for DX10 header
		uint32_t fourCC = *reinterpret_cast<uint32_t*>(header + 84);
		if (fourCC == 0x30315844)
		{  // "DX10"
			// Read DX10 header
			char dx10Header[20];
			fread(dx10Header, 1, 20, file);
			if (!file) return false;

			uint32_t dxgiFormat = *reinterpret_cast<uint32_t*>(dx10Header);
			// Check for formats that support alpha
			switch (dxgiFormat)
			{
				case 2:  // DXGI_FORMAT_R32G32B32A32_FLOAT
				case 10: // DXGI_FORMAT_R16G16B16A16_FLOAT
				case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
				case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
				case 98: // DXGI_FORMAT_BC7_UNORM
				case 99: // DXGI_FORMAT_BC7_UNORM_SRGB
				case 74: // DXGI_FORMAT_BC1_UNORM (potentially has 1-bit alpha)
				case 75: // DXGI_FORMAT_BC1_UNORM_SRGB
				case 76: // DXGI_FORMAT_BC2_UNORM
				case 77: // DXGI_FORMAT_BC2_UNORM_SRGB
				case 78: // DXGI_FORMAT_BC3_UNORM
				case 79: // DXGI_FORMAT_BC3_UNORM_SRGB
					return true;
			}
		}

		return false;
	}

	//ImageFormat NvttToVkFormat(nvtt::Format format, bool srgb, uint32_t components)
	//{
	//	if (components == 1)
	//		return ImageFormat::BC4_UNORM;

	//	switch (format)
	//	{
	//		case nvtt::Format_BC1:
	//		{
	//			if (components == 3)
	//				return srgb ? ImageFormat::BC1_RGB_SRGB : ImageFormat::BC1_RGB_UNORM;
	//			else if (components == 4)
	//				return srgb ? ImageFormat::BC1_RGBA_SRGB : ImageFormat::BC1_RGBA_UNORM;
	//			break;
	//		}
	//		case nvtt::Format_BC2: return srgb ? ImageFormat::BC2_SRGB : ImageFormat::BC2_UNORM;
	//		case nvtt::Format_BC3: return srgb ? ImageFormat::BC3_SRGB : ImageFormat::BC2_UNORM;
	//		case nvtt::Format_BC4:
	//			if (!srgb)
	//				return ImageFormat::BC4_UNORM;
	//			break;
	//		case nvtt::Format_BC4S:
	//			if (!srgb)
	//				return ImageFormat::BC4_SNORM;
	//			break;
	//		case nvtt::Format_BC5:
	//			//if (!srgb)
	//			return ImageFormat::BC5_UNORM;
	//			break;
	//		case nvtt::Format_BC5S:
	//			if (!srgb)
	//				return ImageFormat::BC5_SNORM;
	//			break;

	//		case nvtt::Format_BC6U:
	//			if (!srgb)
	//				return ImageFormat::BC6H_UFLOAT;
	//			break;
	//		case nvtt::Format_BC6S:
	//			if (!srgb)
	//				return ImageFormat::BC6H_SFLOAT;
	//			break;
	//		case nvtt::Format_BC7: return srgb ? ImageFormat::BC7_SRGB : ImageFormat::BC7_UNORM;

	//	}
	//	BEY_CORE_VERIFY(false, "Unknown format!");
	//	return {};
	//}

#if 1

	//CMP_FORMAT UnCompressionFormat(const ImageFormat format)
	//{
	//	switch (format)
	//	{
	//		case ImageFormat::RGBA: return CMP_FORMAT_RGBA_8888;
	//		case ImageFormat::RGBA16F: return CMP_FORMAT_RGBA_16F;
	//		case ImageFormat::RGBA32F: return CMP_FORMAT_RGBA_32F;
	//	}
	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}

	//CMP_FORMAT CompressionFormat(const TextureUsageType usageType, const ImageFormat format)
	//{
	//	switch (usageType)
	//	{
	//		case TextureUsageType::Normal: return CMP_FORMAT_BC1;
	//		case TextureUsageType::Albedo: return CMP_FORMAT_BC1;
	//		case TextureUsageType::MetalnessRoughness: return CMP_FORMAT_BC4;
	//	}
	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}

	ImageFormat CMPToVkFormat(CMP_FORMAT format, bool srgb, uint32_t components)
	{
		switch (format)
		{
			case CMP_FORMAT_BC1:
			{
				if (components == 3)
					return srgb ? ImageFormat::BC1_RGB_SRGB : ImageFormat::BC1_RGB_UNORM;
				else if (components == 4)
					return srgb ? ImageFormat::BC1_RGBA_SRGB : ImageFormat::BC1_RGBA_UNORM;
				return ImageFormat::BC1_RGB_UNORM;
				break;
			}
			case CMP_FORMAT_BC2: return srgb ? ImageFormat::BC2_SRGB : ImageFormat::BC2_UNORM;
			case CMP_FORMAT_BC3: return srgb ? ImageFormat::BC3_SRGB : ImageFormat::BC2_UNORM;
			case CMP_FORMAT_BC4:
				if (!srgb) return ImageFormat::BC4_UNORM;
				break;
			case CMP_FORMAT_BC4_S:
				if (!srgb)
					return ImageFormat::BC4_SNORM;
				break;
			case CMP_FORMAT_BC5:
				if (!srgb)
					return ImageFormat::BC5_UNORM;
				break;
			case CMP_FORMAT_BC5_S:
				if (!srgb)
					return ImageFormat::BC5_SNORM;
				break;
			case CMP_FORMAT_BC6H:
				if (!srgb)
					return ImageFormat::BC6H_UFLOAT;
				break;
			case CMP_FORMAT_BC6H_SF:
				if (!srgb)
					return ImageFormat::BC6H_SFLOAT;
				break;
			case CMP_FORMAT_BC7: return srgb ? ImageFormat::BC7_SRGB : ImageFormat::BC7_UNORM;

		}
		BEY_CORE_VERIFY(false, "Unknown format!");
		return {};
	}

	std::vector<Buffer> TextureImporter::CompressTexture(std::filesystem::path& path, TextureSpecification& spec, uint32_t mipLevels, const eastl::string& compressFormat)
	{
		std::string argument = fmt::format(R"(-fd {} -silent -noprogress -miplevels {}  -DecodeWith OpenGL -EncodeWith GPU "{}" "{}.dds")", compressFormat, mipLevels, path, path);

		ProcessInfo processInfo;
		processInfo.FilePath = "Tools/CompressonatorCLI/compressonatorcli.exe";
		processInfo.CommandLine = argument;
		processInfo.WorkingDirectory = std::filesystem::current_path();
		processInfo.Detached = false;
		processInfo.WaitToFinish = true;
		ProcessHelper::CreateProcess(processInfo);


		return ReadCompressedTexture(path.concat(".dds"), spec);
	}
#else 

	nvtt::Format GetFormat(ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::RGB: return nvtt::Format_RGB;
			case ImageFormat::RGBA: return nvtt::Format_RGBA;
		}
		BEY_CORE_VERIFY(false, "Unknown format!");
		return {};
	}

	nvtt::Format  CompressionFormat(const TextureUsageType usageType, const ImageFormat format)
	{
		switch (usageType)
		{
			case TextureUsageType::Normal: return nvtt::Format_BC1;
			case TextureUsageType::Albedo: return nvtt::Format_BC1;
			case TextureUsageType::MetalnessRoughness: return nvtt::Format_BC4;
		}
		BEY_CORE_VERIFY(false, "Unknown Format!");
		return {};
	}

	eastl::string  CompressionFormatString(const ImageFormat format)
	{
		switch (format)
		{
			case ImageFormat::BC1_RGBA_SRGB:	return "BC1_RGBA_SRGB";
			case ImageFormat::BC1_RGB_UNORM:	return "BC1_RGB_UNORM";
			case ImageFormat::BC1_RGB_SRGB:		return "BC1_RGB_SRGB";
			case ImageFormat::BC1_RGBA_UNORM:	return "BC1_RGBA_UNORM";
			case ImageFormat::BC2_UNORM:		return "BC2_UNORM";
			case ImageFormat::BC2_SRGB:			return "BC2_SRGB";
			case ImageFormat::BC3_UNORM:		return "BC3_UNORM";
			case ImageFormat::BC3_SRGB:			return "BC3_SRGB";
			case ImageFormat::BC4_UNORM:		return "BC4_UNORM";
			case ImageFormat::BC4_SNORM:		return "BC4_SNORM";
			case ImageFormat::BC5_UNORM:		return "BC5_UNORM";
			case ImageFormat::BC5_SNORM:		return "BC5_SNORM";
			case ImageFormat::BC6H_UFLOAT:		return "BC6H_UFLOAT";
			case ImageFormat::BC6H_SFLOAT:		return "BC6H_SFLOAT";
			case ImageFormat::BC7_UNORM:		return "BC7_UNORM";
			case ImageFormat::BC7_SRGB:			return "BC7_SRGB";
		}
		BEY_CORE_VERIFY(false, "Unknown Format!");
		return {};
	}

	//eastl::string  CompressionFormatString(const nvtt::Format format)
	//{
	//	switch (format)
	//	{
	//	case nvtt::Format_BC1:	return "BC1_RGBA_SRGB";
	//		case nvtt::Format_BC1a:	return "BC1_RGB_UNORM";
	//		case nvtt::BC1_RGB_SRGB:		return "BC1_RGB_SRGB";
	//		case nvtt::BC1_RGBA_UNORM:	return "BC1_RGBA_UNORM";
	//		case nvtt::BC2_UNORM:		return "BC2_UNORM";
	//		case nvtt::BC2_SRGB:			return "BC2_SRGB";
	//		case nvtt::BC3_UNORM:		return "BC3_UNORM";
	//		case nvtt::BC3_SRGB:			return "BC3_SRGB";
	//		case nvtt::BC4_UNORM:		return "BC4_UNORM";
	//		case nvtt::BC4_SNORM:		return "BC4_SNORM";
	//		case nvtt::BC5_UNORM:		return "BC5_UNORM";
	//		case nvtt::BC5_SNORM:		return "BC5_SNORM";
	//		case nvtt::BC6H_UFLOAT:		return "BC6H_UFLOAT";
	//		case nvtt::BC6H_SFLOAT:		return "BC6H_SFLOAT";
	//		case nvtt::BC7_UNORM:		return "BC7_UNORM";
	//		case nvtt::BC7_SRGB:			return "BC7_SRGB";
	//	}
	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}

	//ImageFormat CompressionFormatString(const eastl::string& str)
	//{

	//	switch (str)
	//	{
	//		case	"BC1_RGBA_SRGB":		return ImageFormat::BC1_RGBA_SRGB;
	//		case 	"BC1_RGB_UNORM":		return ImageFormat::BC1_RGB_UNORM;
	//		case 	"BC1_RGB_SRGB":			return ImageFormat::BC1_RGB_SRGB;
	//		case 	"BC1_RGBA_UNORM":		return ImageFormat::BC1_RGBA_UNORM;
	//		case 	"BC2_UNORM":			return ImageFormat::BC2_UNORM;
	//		case 	"BC2_SRGB":				return ImageFormat::BC2_SRGB;
	//		case 	"BC3_UNORM":			return ImageFormat::BC3_UNORM;
	//		case 	"BC3_SRGB":				return ImageFormat::BC3_SRGB;
	//		case 	"BC4_UNORM":			return ImageFormat::BC4_UNORM;
	//		case 	"BC4_SNORM":			return ImageFormat::BC4_SNORM;
	//		case 	"BC5_UNORM":			return ImageFormat::BC5_UNORM;
	//		case 	"BC5_SNORM":			return ImageFormat::BC5_SNORM;
	//		case 	"BC6H_UFLOAT":			return ImageFormat::BC6H_UFLOAT;
	//		case 	"BC6H_SFLOAT":			return ImageFormat::BC6H_SFLOAT;
	//		case 	"BC7_UNORM":			return ImageFormat::BC7_UNORM;
	//		case 	"BC7_SRGB":				return ImageFormat::BC7_SRGB;

	//	}
	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}


	struct MyOutputHandler : nvtt::OutputHandler
	{
		/// Destructor.
		virtual ~MyOutputHandler() {}

		MyOutputHandler(const eastl::string& path)
		{
			Stream.open(path, std::ofstream::out | std::ofstream::binary);
		}

		/// Indicate the start of a new compressed image that's part of the final texture.
		virtual void beginImage(int size, int width, int height, int depth, int face, int miplevel);

		/// Output data. Compressed data is output as soon as it's generated to minimize memory allocations.
		virtual bool writeData(const void* data, int size);

		/// Indicate the end of the compressed image. (New in NVTT 2.1)
		virtual void endImage();

		std::map<uint32_t, Buffer> Buffers;
		std::ofstream Stream;
		uint32_t CurrentMip = 0;
	private:
	};

	void MyOutputHandler::beginImage(int size, int width, int height, int depth, int face, int miplevel)
	{
		//Buffers.emplace_back();
		Buffers[miplevel].Allocate(size);
	}

	bool MyOutputHandler::writeData(const void* data, int size)
	{
		auto str = std::basic_string<std::byte>((const std::byte*)data, size);
		Stream.write((const char*)str.data(), str.size());
		if (size == 128)
			return true;
		//BEY_CORE_VERIFY(size != 128);
		std::memcpy(Buffers[CurrentMip].Data, data, size);
		return true;
	}

	void MyOutputHandler::endImage()
	{
		Stream.close();
	}

	std::vector<Buffer> TextureImporter::CompressTexture(const std::filesystem::path& path, Buffer imageBuffer, TextureUsageType usageType, ImageFormat format, uint32_t width, uint32_t height)
	{
		// Create the compression context; enable CUDA compression, so that
		// CUDA-capable GPUs will use GPU acceleration for compression, with a
		// fallback on other GPUs for CPU compression.
		nvtt::Context context(true);

		auto nvFormat = CompressionFormat(usageType, format);

		nvtt::CompressionOptions compressionOptions;
		compressionOptions.setFormat(nvFormat);
		compressionOptions.setQuality(nvtt::Quality_Fastest);


		nvtt::OutputOptions outputOptions;
		//outputOptions.setSrgbFlag(usageType == TextureUsageType::Albedo);
		//outputOptions.setFileName(fmt::format("{}.dds", path.generic_string()).c_str());

		std::unique_ptr<MyOutputHandler> handler = std::make_unique<MyOutputHandler>(fmt::format("{}.dds", path.generic_string()).c_str());
		outputOptions.setOutputHandler(handler.get());


		//std::ofstream ostream(path.string() + "compressed");

		nvtt::RefImage refImage;
		refImage.width = width;
		refImage.height = height;
		refImage.channel_interleave = true;
		refImage.data = imageBuffer.Data;
		refImage.depth = 1;
		refImage.num_channels = 4;

		nvtt::EncodeSettings settings;
		settings.format = CompressionFormat(usageType, format);
		settings.quality = nvtt::Quality_Fastest;

		settings.SetRGBPixelType(nvtt::PixelType_UnsignedNorm);
		settings.SetFormat(CompressionFormat(usageType, format));

		nvtt::CPUInputBuffer inputBuffer(&refImage, nvtt::UINT8);


		//std::vector<Buffer> outputs;

		uint32_t sizeInBytes = context.estimateSize(width, height, 1, 1, compressionOptions);
		//handler->Buffers.emplace_back();
		handler->Buffers[0].Allocate(sizeInBytes);


		nvtt::Surface surface;
		surface.setImage2D(settings.format, width, height, handler->Buffers[0].Data);

		uint32_t mips = surface.countMipmaps();


		nvtt::nvtt_encode(inputBuffer, handler->Buffers[0].Data, settings);
		//// Write the DDS header.
		//if (!context.outputHeader(surface, mips, compressionOptions, outputOptions))
		//{
		//	return {};
		//}
		//surface.setImage2D(settings.format, width, height, handler->Buffers[0].Data);


		//for (uint32_t mip = 0; mip < mips; mip++)
		//{
		//	
		//	// Compress this image and write its data.
		//	if (!context.compress(surface, 0, mip, compressionOptions, outputOptions))
		//	{
		//		return {};
		//	}
		//	surface.setImage2D(settings.format, width, height, handler->Buffers[mip].Data);
		//	surface.save("asdf.dds");
		//	//surface.setImage2D(settings.format, width >> mip, height >> mip, handler->Buffers[mip].Data);
		//	handler->CurrentMip++;
		//	//handler->Buffers[handler->CurrentMip].Allocate(sizeInBytes);

		//	if (mip == mips - 1) break;

		//	// Resize the image to the next mipmap size.
		//	// NVTT has several mipmapping filters; Box is the lowest-quality, but
		//	// also the fastest to use.
		//	surface.buildNextMipmap(nvtt::MipmapFilter_Box);
		//	// For general image resizing. use image.resize().

		//}

		CMP_MipSet MipSetIn;
		memset(&MipSetIn, 0, sizeof(CMP_MipSet));
		if (CMP_LoadTexture(pszSourceFile, &MipSetIn) != CMP_OK)
		{
			std::printf("Error: Loading source file!\n");
		}

		////-----------------------------------------------------
		//// when using GPU: The texture must have width and height as a multiple of 4
		//// Check texture for width and height
		////-----------------------------------------------------
		//if ((MipSetIn.m_nWidth % 4) > 0 || (MipSetIn.m_nHeight % 4) > 0)
		//{
		//	std::printf("Error: Texture width and height must be multiple of 4\n");
		//}

		KernelOptions   kernel_options;
		memset(&kernel_options, 0, sizeof(KernelOptions));

		kernel_options.encodeWith = CMP_GPU_VLK;         // Using OpenCL GPU Encoder, can replace with DXC for DirectX
		kernel_options.format = CMP_FORMAT_BC1;          // Set the format to process
		kernel_options.fquality = 1.0f;            // Set the quality of the result



		CMP_Texture srcTexture;
		srcTexture.dwSize = sizeof(srcTexture);
		srcTexture.dwWidth = width;
		srcTexture.dwHeight = height;
		srcTexture.dwPitch = 0;
		srcTexture.format = CMP_FORMAT_BC1;
		srcTexture.dwDataSize = (uint32_t)handler->Buffers[0].Size;
		srcTexture.pData = (CMP_BYTE*)handler->Buffers[0].Data;

		//--------------------------------------------------------------
		// Setup a results buffer for the processed file,
		// the content will be set after the source texture is processed
		// in the call to CMP_ProcessTexture()
		//--------------------------------------------------------------
		CMP_MipSet MipSetCmp;
		memset(&MipSetCmp, 0, sizeof(CMP_MipSet));

		//===============================================
		// Compress the texture using Framework Lib
		//===============================================
		auto res = CMP_ProcessTexture(&MipSetIn, &MipSetCmp, kernel_options, CompressionCallback);

		std::vector<Buffer> buffers;
		for (auto g : handler->Buffers | std::ranges::views::values)
			buffers.push_back(g);
		return buffers;
	}


#endif


	//static int BlockSize(nvtt::Format format)
	//{
	//	if (format == nvtt::Format_DXT1 || format == nvtt::Format_DXT1a || format == nvtt::Format_DXT1n)
	//	{
	//		return 8;
	//	}
	//	else if (format == nvtt::Format_DXT3)
	//	{
	//		return 16;
	//	}
	//	else if (format == nvtt::Format_DXT5 || format == nvtt::Format_DXT5n || format == nvtt::Format_BC3_RGBM)
	//	{
	//		return 16;
	//	}
	//	else if (format == nvtt::Format_BC4)
	//	{
	//		return 8;
	//	}
	//	else if (format == nvtt::Format_BC5 /*|| format == Format_BC5_Luma*/)
	//	{
	//		return 16;
	//	}
	//	else if (format == nvtt::Format_CTX1)
	//	{
	//		return 8;
	//	}
	//	else if (format == nvtt::Format_BC6U || format == nvtt::Format_BC6S)
	//	{
	//		return 16;
	//	}
	//	else if (format == nvtt::Format_BC7)
	//	{
	//		return 16;
	//	}
	//	BEY_CORE_ASSERT(false);
	//	return 0;
	//}


	//static uint32_t ComputeImageSize(uint32_t w, uint32_t h, uint32_t d, nvtt::Format format)
	//{
	//	return ((w + 3) / 4) * ((h + 3) / 4) * BlockSize(format) * d;
	//}

	//// Function to read DDS header and return nvtt::Format
	//nvtt::Format ReadFormatFromDDS(const eastl::string& path)
	//{
	//	eastl::string result;
	//	std::ifstream in(path, std::ios::in | std::ios::binary);
	//	if (in)
	//	{
	//		in.seekg(0, std::ios::beg);
	//		result.resize(0x80);
	//		in.read(result.data(), 0x80);
	//	}
	//	in.close();

	//	// DDS header starts at byte 12 of the file
	//	const uint32_t DDS_MAGIC = 0x20534444;
	//	uint32_t magic;
	//	memcpy(&magic, result.data(), sizeof(uint32_t));

	//	BEY_CORE_ASSERT(magic == DDS_MAGIC);


	//	uint32_t pfFlags;
	//	memcpy(&pfFlags, result.data() + 0x54, sizeof(uint32_t));

	//	// Determine the format
	//	if (pfFlags & 0x54584440)
	//	{
	//		return nvtt::Format_BC1;
	//	}
	//	else if (pfFlags & 0x41)
	//	{
	//		return nvtt::Format_BC2;
	//	}
	//	else if (pfFlags & 0x40)
	//	{
	//		return nvtt::Format_BC3;
	//	}
	//	else if (pfFlags & 0x20000)
	//	{
	//		return nvtt::Format_BC4;
	//	}
	//	else if (pfFlags & 0x80000)
	//	{
	//		return nvtt::Format_BC5;
	//	}
	//	else if (pfFlags & 0x2000000)
	//	{
	//		return nvtt::Format_BC6S;
	//	}
	//	else if (pfFlags & 0x8000000)
	//	{
	//		return nvtt::Format_BC7;
	//	}

	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}

	//nvtt::Format NvttFormat(gli::format format)
	//{
	//	switch (format)
	//	{
	//		case gli::FORMAT_RGB_DXT1_UNORM_BLOCK8:		return nvtt::Format_DXT1;
	//		case gli::FORMAT_RGB_DXT1_SRGB_BLOCK8:		return nvtt::Format_DXT1;
	//		case gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8:	return nvtt::Format_DXT1;
	//		case gli::FORMAT_RGBA_DXT1_SRGB_BLOCK8:		return nvtt::Format_DXT1;
	//		case gli::FORMAT_RGBA_DXT3_UNORM_BLOCK16:	return nvtt::Format_DXT3;
	//		case gli::FORMAT_RGBA_DXT3_SRGB_BLOCK16:	return nvtt::Format_DXT3;
	//		case gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16:	return nvtt::Format_DXT5;
	//		case gli::FORMAT_RGBA_DXT5_SRGB_BLOCK16:	return  nvtt::Format_DXT5;
	//	}
	//	BEY_CORE_VERIFY(false);
	//	return {};
	//}

	static void tinyktxCallbackError(void* user, char const* msg)
	{
		BEY_CORE_ERROR("Tiny_Ktx ERROR: {}", msg);
	}
	static void* tinyktxCallbackAlloc(void* user, size_t size)
	{
		return Allocator::Allocate(size);
	}
	static void tinyktxCallbackFree(void* user, void* data)
	{
		Allocator::Free(data);
	}
	static size_t tinyktxCallbackRead(void* user, void* data, size_t size)
	{
		auto handle = (FILE*)user;
		return fread(data, 1, size, handle);
	}
	static bool tinyktxCallbackSeek(void* user, int64_t offset)
	{
		auto handle = (FILE*)user;
		return fseek(handle, (long)offset, SEEK_SET);

	}
	static int64_t tinyktxCallbackTell(void* user)
	{
		auto handle = (FILE*)user;
		return ftell(handle);
	}

	ImageFormat TinyDDSToImageFormat(TinyDDS_Format format)
	{
		switch (format)
		{
			case TDDS_BC1_RGBA_SRGB_BLOCK: return ImageFormat::BC1_RGBA_SRGB;
			case TDDS_BC1_RGBA_UNORM_BLOCK: return ImageFormat::BC1_RGBA_UNORM;
			case TDDS_BC2_SRGB_BLOCK: return ImageFormat::BC2_SRGB;
			case TDDS_BC2_UNORM_BLOCK: return ImageFormat::BC2_UNORM;
			case TDDS_BC3_SRGB_BLOCK: return ImageFormat::BC3_SRGB;
			case TDDS_BC3_UNORM_BLOCK: return ImageFormat::BC3_UNORM;
			case TDDS_BC4_UNORM_BLOCK: return ImageFormat::BC4_UNORM;
			case TDDS_BC4_SNORM_BLOCK: return ImageFormat::BC4_SNORM;
			case TDDS_BC5_SNORM_BLOCK: return ImageFormat::BC5_SNORM;
			case TDDS_BC5_UNORM_BLOCK: return ImageFormat::BC5_UNORM;
			case TDDS_BC6H_SFLOAT_BLOCK: return ImageFormat::BC6H_SFLOAT;
			case TDDS_BC6H_UFLOAT_BLOCK: return ImageFormat::BC6H_UFLOAT;
			case TDDS_BC7_SRGB_BLOCK: return ImageFormat::BC7_SRGB;
			case TDDS_BC7_UNORM_BLOCK: return ImageFormat::BC7_UNORM;
		}
		BEY_CORE_ASSERT(false);
		return {};
	}

	std::vector<Buffer> TextureImporter::ReadCompressedTexture(std::filesystem::path& path, TextureSpecification& spec)
	{

		std::vector<Buffer> outputs;
#if 1

		TinyDDS_Callbacks callbacks{
			&tinyktxCallbackError,
			&tinyktxCallbackAlloc,
			&tinyktxCallbackFree,
			tinyktxCallbackRead,
			&tinyktxCallbackSeek,
			&tinyktxCallbackTell
		};

		bool hasTransparency = QuickDDSHasTransparency(path.string().c_str());

		FILE* file = fopen(path.string().c_str(), "rb");

		if (file == nullptr)
			return {};

		auto ctx = TinyDDS_CreateContext(&callbacks, file);
		TinyDDS_ReadHeader(ctx);

		spec.Width = TinyDDS_Width(ctx);
		spec.Height = TinyDDS_Height(ctx);
		spec.Format = TinyDDSToImageFormat(TinyDDS_GetFormat(ctx));
		spec.HasTransparency = hasTransparency;

		for (uint32_t mip = 0; mip < TinyDDS_NumberOfMipmaps(ctx); mip++)
		{
			auto size = TinyDDS_ImageSize(ctx, mip);

			outputs.emplace_back(Buffer::Copy(TinyDDS_ImageRawData(ctx, mip), size));
		}
		fclose(file);

#elif 0
		nvtt::SurfaceSet surfaces;
		surfaces.loadDDS(path.string().c_str());



		spec.Format = ImageFormat::BC1_RGBA_SRGB;
		spec.Width = surfaces.GetWidth();
		spec.Height = surfaces.GetHeight();

		nvtt::Context context(false);
		nvtt::CompressionOptions options;
		options.setQuality(nvtt::Quality_Production);
		options.setFormat(nvtt::Format_BC1);

		uint32_t size = context.estimateSize(surfaces.GetWidth(), surfaces.GetHeight(), 1, 1, options);


		for (int mip = 0; mip < surfaces.GetMipmapCount(); ++mip)
		{
			auto surface = surfaces.GetSurface(0, mip);
			surface.ToCPU();
			surface.


				outputs.emplace_back(Buffer::Copy(surface.data(), size));

			size /= 2;

		}


#elif 0

		CMP_MipSet srcMipSet = {};
		CMP_MipSet mipSet = {};

		CMP_ERROR err = CMP_LoadTexture(path.string().c_str(), &srcMipSet);
		if (err != CMP_OK)
			return outputs;

		auto g = CMP_CreateCompressMipSet(&mipSet, &srcMipSet);
		mipSet.m_format = srcMipSet.m_format;

		spec.Format = CMPToVkFormat(srcMipSet.m_format, spec.SRGB, srcMipSet.m_nChannels);
		spec.Width = mipSet.m_nWidth;
		spec.Height = mipSet.m_nHeight;


		for (int mip = 0; mip < srcMipSet.m_nMipLevels; mip++)
		{

			CMP_MipLevel* level = new CMP_MipLevel;
			CMP_GetMipLevel(&level, &mipSet, mip, 1);

			//const auto level = mipSet.m_pMipLevelTable[mip];

			outputs.emplace_back(Buffer::Copy(level->m_pbData, level->m_dwLinearSize));
		}

#else
		auto texture = gli::load_dds(path.string().c_str());


		glm::ivec3 const extent(texture.extent());
		spec.Width = extent.x;
		spec.Height = extent.y;


		auto nvttFormat = NvttFormat(texture.format());
		spec.Format = NvttToVkFormat(nvttFormat, gli::is_srgb(texture.format()), (uint32_t)gli::component_count(texture.format()));

		//const uint32_t size = (uint32_t)texture.size();

		//uint32_t const FaceTotal = (uint32_t)(texture.layers() * texture.faces());


		for (std::size_t Layer = 0; Layer < texture.layers(); ++Layer)
		{
			for (std::size_t Face = 0; Face < texture.faces(); ++Face)
			{
				for (std::size_t Level = 0; Level < texture.levels(); ++Level)
				{
					outputs.emplace_back();

					int const LayerGL = static_cast<int>(Layer);
					glm::tvec3<int> Extent(texture.extent(Level));

					switch (texture.target())
					{

						case gli::TARGET_1D_ARRAY:
						case gli::TARGET_2D:
						case gli::TARGET_CUBE:
							if (gli::is_compressed(texture.format()))
							{
								uint32_t mipSize = (uint32_t)texture.size(Level);
								outputs[Level].Allocate(mipSize);
								std::memcpy(outputs[Level].Data, texture.data(Layer, Face, Level), mipSize);
							}
							else
								assert(0);

							break;

						default: assert(0);
					}
				}
			}
		}
#endif

		return outputs;

	}


	eastl::string CompressionFormatStr(const TextureUsageType usageType, const uint32_t channels)
	{
		switch (usageType)
		{
			case TextureUsageType::Normal: return "BC5";
			case TextureUsageType::Albedo: return channels == 4 ? "BC7" : "BC1";
			case TextureUsageType::MetalnessRoughness: return  channels > 1 ? "BC1" : "BC4";
		}
		BEY_CORE_VERIFY(false, "Unknown Format!");
		return {};
	}

	//ImageFormat CompressionFormat(const TextureUsageType usageType, const uint32_t channels)
	//{
	//	switch (usageType)
	//	{
	//		case TextureUsageType::Normal: return ImageFormat::BC5_UNORM;
	//		case TextureUsageType::Albedo: return channels == 4 ? ImageFormat::BC7_SRGB : ImageFormat::BC1_RGBA_SRGB;
	//		case TextureUsageType::MetalnessRoughness: return channels > 1 ? ImageFormat::BC1_RGB_UNORM : ImageFormat::BC4_UNORM;
	//	}
	//	BEY_CORE_VERIFY(false, "Unknown Format!");
	//	return {};
	//}

	std::vector<Buffer> TextureImporter::ToBufferFromFile(std::filesystem::path& path, std::atomic_bool& found, TextureSpecification& spec)
	{
		std::vector<Buffer> imageBuffers;
		std::string pathString = path.string();
		bool isSRGB = (spec.Format == ImageFormat::SRGB) || (spec.Format == ImageFormat::SRGBA);
		int width, height, channels;
		if (!std::filesystem::exists(pathString + ".dds"))
		{
			if (!spec.Compress)
			{
				imageBuffers.emplace_back();
				auto& imageBuffer = imageBuffers[0];
				if (stbi_is_hdr(pathString.c_str()))
				{
					found = true;
					imageBuffer.Data = (byte*)stbi_loadf(pathString.c_str(), &width, &height, &channels, 4);
					imageBuffer.Size = width * height * 4 * sizeof(float);
					spec.Format = ImageFormat::RGBA32F;
				}
				else if (stbi_is_16_bit(pathString.c_str()))
				{
					found = true;
					imageBuffer.Data = stbi_load_16(pathString.c_str(), &width, &height, &channels, 4);
					imageBuffer.Size = width * height * 4 * sizeof(uint16_t);
					spec.Format = ImageFormat::RGBA16F;
				}
				else
				{
					//stbi_set_flip_vertically_on_load(1);
					found = true;
					imageBuffer.Data = stbi_load(pathString.c_str(), &width, &height, &channels, 4);
					imageBuffer.Size = width * height * 4;
					spec.Format = ImageFormat::RGBA;
				}

				if (!imageBuffer.Data)
					return {};

				found = true;
				spec.Width = width;
				spec.Height = height;
			}
			else
			{
				bool alreadyCompressed = false;
				if (pathString.ends_with(".dds"))
				{
					TinyDDS_Callbacks callbacks{
						&tinyktxCallbackError,
						&tinyktxCallbackAlloc,
						&tinyktxCallbackFree,
						tinyktxCallbackRead,
						&tinyktxCallbackSeek,
						&tinyktxCallbackTell
					};

					FILE* file = fopen(path.string().c_str(), "rb");

					if (file == nullptr)
						return {};

					auto ctx = TinyDDS_CreateContext(&callbacks, file);
					TinyDDS_ReadHeader(ctx);

					width = TinyDDS_Width(ctx);
					height = TinyDDS_Height(ctx);
					alreadyCompressed = IsCompressedFormat(TinyDDSToImageFormat(TinyDDS_GetFormat(ctx)));

					fclose(file);
				}
				else
					stbi_info(pathString.c_str(), &width, &height, &channels);

				if (alreadyCompressed)
					imageBuffers = ReadCompressedTexture(path, spec);
				else //NOTE: - 1 because we compressed images can't have mip maps in some cases depending on their size.
					imageBuffers = CompressTexture(path, spec, glm::max(Utils::CalculateMipCount(width, height) - 3, 1), CompressionFormatStr(spec.UsageType, channels));
				found = true;
			}

		}
		else
		{
			found = true;
			imageBuffers = ReadCompressedTexture(path.concat(".dds"), spec);
		}

		return imageBuffers;
	}

	Buffer TextureImporter::ToBufferFromMemory(Buffer buffer, TextureSpecification& spec)
	{
		Buffer imageBuffer;

		int width, height, channels;
		bool isSRGB = (spec.Format == ImageFormat::SRGB) || (spec.Format == ImageFormat::SRGBA);
		if (stbi_is_hdr_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size))
		{
			imageBuffer.Data = (byte*)stbi_loadf_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size, &width, &height, &channels, STBI_rgb_alpha);
			imageBuffer.Size = width * height * channels * sizeof(float);
			spec.Format = GetImageFormat(channels, isSRGB, true, false);
		}
		else if (stbi_is_16_bit_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size))
		{
			imageBuffer.Data = stbi_load_16_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size, &width, &height, &channels, STBI_rgb_alpha);
			imageBuffer.Size = width * height * channels * sizeof(uint16_t);
			spec.Format = GetImageFormat(channels, isSRGB, false, true);
		}
		else
		{
			// stbi_set_flip_vertically_on_load(1);
			imageBuffer.Data = stbi_load_from_memory((const stbi_uc*)buffer.Data, (int)buffer.Size, &width, &height, &channels, STBI_rgb_alpha);
			if (channels == 3)
				channels = 4; // Because RGB8 is unsupported
			imageBuffer.Size = width * height * channels;
			spec.Format = GetImageFormat(channels, isSRGB, false, false);
		}

		if (!imageBuffer.Data)
			return {};

		// Just double check
		if (!spec.HasTransparency && spec.UsageType == TextureUsageType::Albedo && channels == 4)
		{
			int BPP = (int)imageBuffer.Size / (width * height * channels);
			for (int i = 0; i < width * height; ++i)
			{
				uint8_t* pixel = static_cast<uint8_t*>(imageBuffer.Data) + i * BPP;
				uint8_t alpha = pixel[3];
				if (alpha < 255)
				{
					spec.HasTransparency = true;
					break; // Exit the loop early if transparency is found
				}
			}
		}

		//if (spec.Compress)
		//{
		//	imageBuffers = CompressTexture({}, imageBuffer, spec.UsageType, spec.Format, width, height);
		//	spec.Format = CMPToVkFormat(CompressionFormat(spec.UsageType, spec.Format), spec.SRGB, channels);
		//}
		spec.Width = width;
		spec.Height = height;
		return imageBuffer;
	}

	// Helper function to extract the bits and convert them to float
	float UnpackUnsignedFloat(uint32_t value, uint32_t bitSize, int exponentBits)
	{
		// Isolate the bits for the given field (mantissa + exponent)
		uint32_t mask = (1u << bitSize) - 1;
		uint32_t field = value & mask;

		// Extract the exponent and mantissa based on IEEE-like format
		uint32_t exponent = (field >> (bitSize - exponentBits)) & ((1u << exponentBits) - 1);
		uint32_t mantissa = field & ((1u << (bitSize - exponentBits)) - 1);

		// If the exponent is non-zero, calculate the float value
		if (exponent != 0)
		{
			return std::ldexp(static_cast<float>(mantissa + (1u << (bitSize - exponentBits))),
							  static_cast<int>(exponent) - ((1 << (exponentBits - 1)) - 1) - (bitSize - exponentBits));
		}

		// If exponent is zero, this is a denormalized number or zero
		return std::ldexp(static_cast<float>(mantissa), -((1 << (exponentBits - 1)) - 1) - (bitSize - exponentBits));
	}

	void TextureImporter::WriteImageToFile(const std::filesystem::path& path, const ImageSpecification& spec, Buffer buffer)
	{
		//BEY_SCOPE_TIMER("WriteImageToFile, path: {}", path.string());

		if (!std::filesystem::exists(path))
			std::filesystem::create_directory(path);

		char filename[64];
		time_t t = time(NULL);
		struct tm* tm = localtime(&t);
		strftime(filename, sizeof(filename), "image_%Y%m%d%H%M%S", tm);
		stbi_flip_vertically_on_write(1);

		uint32_t comps;
		switch (spec.Format)
		{
			case ImageFormat::RGBA32F:
			{
				comps = 4;
#if 1
				stbi_write_hdr(fmt::format("{}/HDR/{}.hdr", path.string(), filename).c_str(), spec.Width, spec.Height, comps, (float*)buffer.Data);
#endif

				// Convert float data to unsigned char
				auto* data = hnew std::byte[spec.Width * spec.Height * comps];
				for (uint32_t i = 0; i < spec.Width * spec.Height * comps; i++)
				{
					data[i] = (std::byte)(255.0f * ((float*)buffer.Data)[i]);
				}
				//stbi_write_bmp(fmt::format("{}/{}.bmp", path.string(), filename).c_str(), spec.Width, spec.Height, comps, data);
				stbi_write_jpg(fmt::format("{}/{}.jpg", path.string(), filename).c_str(), spec.Width, spec.Height, comps, data, 100);
				hdelete[] data;
				break;
			}
			case ImageFormat::B10G11R11UFLOAT:
			{
				comps = 3; // Set the number of components to 3 (B10G11R11UFLOAT has 3 channels)

				auto* data = hnew float[spec.Width * spec.Height * comps];
				for (size_t i = 0; i < spec.Width * spec.Height; i++)
				{
					// Extract the B10G11R11UFLOAT data and pack it into bytes
					const uint32_t packedValue = buffer.Read<uint32_t>(i * sizeof(uint32_t));

					// Extract the components from the packed value
					uint32_t rBits = (packedValue >> 0) & 0x7FF;   // 11 bits for R (bits 0-10)
					uint32_t gBits = (packedValue >> 11) & 0x7FF;  // 11 bits for G (bits 11-21)
					uint32_t bBits = (packedValue >> 22) & 0x3FF;  // 10 bits for B (bits 22-31)

					data[i * comps + 0] = UnpackUnsignedFloat(rBits, 11, 5);   // 11 bits for R, 5 exponent bits
					data[i * comps + 1] = UnpackUnsignedFloat(gBits, 11, 5);   // 11 bits for G, 5 exponent bits
					data[i * comps + 2] = UnpackUnsignedFloat(bBits, 10, 5);   // 10 bits for B, 5 exponent bits
				}

				stbi_write_hdr(fmt::format("{}/{}.hdr", path.string(), filename).c_str(), spec.Width, spec.Height, comps, data);

				// Convert the float buffer to uint8_t in-place for JPEG (overwrite the float data)
				for (size_t i = 0; i < spec.Width * spec.Height; i++)
				{
					((std::byte*)data)[i * comps + 0] = static_cast<std::byte>(std::clamp(data[i * comps + 0] * 255.0f, 0.0f, 255.0f));
					((std::byte*)data)[i * comps + 1] = static_cast<std::byte>(std::clamp(data[i * comps + 1] * 255.0f, 0.0f, 255.0f));
					((std::byte*)data)[i * comps + 2] = static_cast<std::byte>(std::clamp(data[i * comps + 2] * 255.0f, 0.0f, 255.0f));
				}

				// Now, write the JPEG using the same buffer (note: data is now treated as uint8_t)
				stbi_write_jpg(fmt::format("{}/{}.jpg", path.string(), filename).c_str(), spec.Width, spec.Height, comps, data, 100);

				hdelete[] data;
				break;
			}
			default:
				BEY_CORE_VERIFY(false, "Unknown Format!");
		}
	}
}
