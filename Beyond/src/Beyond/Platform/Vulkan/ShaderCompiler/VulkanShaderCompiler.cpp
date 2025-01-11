#include "pch.h"
#include "VulkanShaderCompiler.h"

#include <codecvt>


#include "Beyond/Utilities/StringUtils.h"

#include "ShaderPreprocessing/GlslIncluder.h"
#include "ShaderPreprocessing/HlslIncluder.h"

#include "VulkanShaderCache.h"

#include <spirv_cross/spirv_glsl.hpp>
#include <spirv-tools/optimizer.hpp>
#include <spirv-tools/libspirv.h>

#include <dxc/dxcapi.h>
#include <shaderc/shaderc.hpp>
#include <libshaderc_util/file_finder.h>

#include "Beyond/Core/Hash.h"

#include "Beyond/Platform/Vulkan/VulkanShader.h"
#include "Beyond/Platform/Vulkan/VulkanContext.h"

#include "Beyond/Serialization/FileStream.h"

#include <cstdlib>

#include "Beyond/ImGui/ImGuiUtilities.h"

#if defined(BEY_PLATFORM_LINUX)
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace Beyond {

	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::UniformBuffer>> s_UniformBuffers; // set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::StorageBuffer>> s_StorageBuffers; // set -> binding point -> buffer
	static std::unordered_map<uint32_t, std::unordered_map<uint32_t, ShaderResource::AccelerationStructure>> s_AccelerationStructures; // set -> binding point -> acceleration structure

	namespace Utils {

		static const char* GetCacheDirectory()
		{
			// TODO: make sure the assets directory is valid
			return "Resources/Cache/Shader/Vulkan";
		}

		static void CreateCacheDirectoryIfNeeded()
		{
			std::string cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}

		static ShaderUniformType SPIRTypeToShaderUniformType(spirv_cross::SPIRType type)
		{
			switch (type.basetype)
			{
				case spirv_cross::SPIRType::Boolean:  return ShaderUniformType::Bool;
				case spirv_cross::SPIRType::Int:
					if (type.vecsize == 1)            return ShaderUniformType::Int;
					if (type.vecsize == 2)            return ShaderUniformType::IVec2;
					if (type.vecsize == 3)            return ShaderUniformType::IVec3;
					if (type.vecsize == 4)            return ShaderUniformType::IVec4;

				case spirv_cross::SPIRType::UInt:     return ShaderUniformType::UInt;
				case spirv_cross::SPIRType::Float:
					if (type.columns == 3)            return ShaderUniformType::Mat3;
					if (type.columns == 4)            return ShaderUniformType::Mat4;

					if (type.vecsize == 1)            return ShaderUniformType::Float;
					if (type.vecsize == 2)            return ShaderUniformType::Vec2;
					if (type.vecsize == 3)            return ShaderUniformType::Vec3;
					if (type.vecsize == 4)            return ShaderUniformType::Vec4;
					break;
				case spirv_cross::SPIRType::Struct:		return ShaderUniformType::Struct;
			}
			BEY_CORE_ASSERT(false, "Unknown type!");
			return ShaderUniformType::None;
		}

		static uint32_t NumDims(spv::Dim dims)
		{
			switch (dims)
			{
				case spv::Dim1D:
				case spv::DimBuffer: return 1;
				case spv::Dim2D:
				case spv::DimSubpassData:
				case spv::DimRect: return 2;
				case spv::Dim3D:
				case spv::DimCube: return 3;
			}
			BEY_CORE_ASSERT(false, "Unknown dimension!");
			return 0;
		}

		static RenderPassInputType RenderPassInputTypeFromReflection(uint32_t dims, bool sampled)
		{
			switch (sampled)
			{
				case true:
					if (dims == 1)
						return RenderPassInputType::ImageSampler1D;
					else if (dims == 2)
						return RenderPassInputType::ImageSampler2D;
					else if (dims == 3)
						return RenderPassInputType::ImageSampler3D;
				case false:
					if (dims == 1)
						return RenderPassInputType::StorageImage1D;
					else if (dims == 2)
						return RenderPassInputType::StorageImage2D;
					else if (dims == 3)
						return RenderPassInputType::StorageImage3D;
			}

			BEY_CORE_ASSERT(false);
			return RenderPassInputType::None;
		}

		uint32_t GetReflectedArraySize(const spirv_cross::SPIRType& type)
		{
			// For static arrays, the size is stored in the array size field
			if (type.array.size() == 1)
			{
				return  type.array[0];
			}
			return 1;
		}

	}

	VulkanShaderCompiler::VulkanShaderCompiler(Ref<VulkanShader> shader, const std::filesystem::path& shaderSourcePath, bool disableOptimization)
		: m_ShaderSourcePath(shaderSourcePath), m_DisableOptimization(disableOptimization), m_Shader(shader)
	{
		m_Language = ShaderUtils::ShaderLangFromExtension(shaderSourcePath.extension().string());
		m_Shader->m_Language = m_Language;
	}

	bool VulkanShaderCompiler::Reload(bool forceCompile)
	{
		m_ShaderSource.clear();
		m_StagesMetadata.clear();
		m_SPIRVDebugData.clear();
		m_SPIRVData.clear();
		m_AcknowledgedMacros.clear();

		Utils::CreateCacheDirectoryIfNeeded();
		const std::string source = Utils::ReadFileAndSkipBOM(m_ShaderSourcePath);
		BEY_CORE_VERIFY(source.size(), "Failed to load shader!");

		BEY_CORE_TRACE_TAG("Renderer", "Compiling shader: {}", m_ShaderSourcePath.string());
		m_ShaderSource = PreProcess(source);
		const VkShaderStageFlagBits changedStages = VulkanShaderCache::HasChanged(this);

		const bool compileSucceeded = CompileOrGetVulkanBinaries(m_SPIRVDebugData, m_SPIRVData, changedStages, forceCompile);
		if (!compileSucceeded)
		{
			//BEY_CORE_VERIFY(false);
			return false;
		}

		// Reflection
		if (forceCompile || changedStages != 0 || !TryReadCachedReflectionData())
		{
			ReflectAllShaderStages(m_SPIRVDebugData);
			SerializeReflectionData();
		}

		return true;
	}

	void VulkanShaderCompiler::ClearUniformBuffers()
	{
		s_UniformBuffers.clear();
		s_StorageBuffers.clear();
		s_AccelerationStructures.clear();
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::PreProcess(const std::string& source)
	{
		switch (m_Language)
		{
			case ShaderUtils::SourceLang::GLSL: return PreProcessGLSL(source);
			case ShaderUtils::SourceLang::HLSL: return PreProcessHLSL(source);
		}

		BEY_CORE_VERIFY(false);
		return {};
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::PreProcessGLSL(const std::string& source)
	{
		std::map<VkShaderStageFlagBits, std::string> shaderSources = ShaderPreprocessor::PreprocessShader<ShaderUtils::SourceLang::GLSL>(source, m_AcknowledgedMacros);

		static shaderc::Compiler compiler;

		shaderc_util::FileFinder fileFinder;
		fileFinder.search_path().emplace_back("Resources/Shaders/Include/GLSL/"); //Main include directory
		fileFinder.search_path().emplace_back("Resources/Shaders/Include/Common/"); //Shared include directory
		for (auto& [stage, shaderSource] : shaderSources)
		{
			shaderc::CompileOptions options;
			options.AddMacroDefinition("__GLSL__");
			options.AddMacroDefinition(ShaderUtils::VKStageToShaderMacro(stage));

			const auto& globalMacros = Renderer::GetGlobalShaderMacros();
			for (const auto& [name, value] : globalMacros)
				options.AddMacroDefinition(name, value);

			// Deleted by shaderc and created per stage
			GlslIncluder* includer = new GlslIncluder(&fileFinder);

			options.SetIncluder(std::make_unique<GlslIncluder>(&fileFinder));
			const auto preProcessingResult = compiler.PreprocessGlsl(shaderSource, ShaderUtils::ShaderStageToShaderC(stage), m_ShaderSourcePath.string().c_str(), options);
			if (preProcessingResult.GetCompilationStatus() != shaderc_compilation_status_success)
				BEY_CORE_ERROR_TAG("Renderer", "Failed to pre-process \"{}\" {} shader.\nError: {}", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage), preProcessingResult.GetErrorMessage());

			m_StagesMetadata[stage].HashValue = Hash::GenerateFNVHash(shaderSource.c_str());
			m_StagesMetadata[stage].Headers = std::move(includer->GetIncludeData());

			m_AcknowledgedMacros.merge(includer->GetParsedSpecialMacros());

			shaderSource = std::string(preProcessingResult.begin(), preProcessingResult.end());
		}
		return shaderSources;
	}

	std::map<VkShaderStageFlagBits, std::string> VulkanShaderCompiler::PreProcessHLSL(const std::string& source)
	{
		std::map<VkShaderStageFlagBits, std::string> shaderSources;
		if (m_Shader->m_ExternalShader)
			shaderSources[ShaderUtils::HLSLShaderProfile(m_Shader->m_TargetProfile.c_str())] = source;
		else
			shaderSources = ShaderPreprocessor::PreprocessShader<ShaderUtils::SourceLang::HLSL>(source, m_AcknowledgedMacros);

#ifdef BEY_PLATFORM_WINDOWS
		std::wstring buffer = m_ShaderSourcePath.wstring();
#else
		std::wstring buffer;
		buffer.resize(m_ShaderSourcePath.string().size() * 2);
		mbstowcs(buffer.data(), m_ShaderSourcePath.string().c_str(), m_ShaderSourcePath.string().size());
#endif

		std::vector<const wchar_t*> arguments{ buffer.c_str(), L"-P", DXC_ARG_WARNINGS_ARE_ERRORS,
			L"-I Resources/Shaders/Include/Common/",
			L"-I Resources/Shaders/Include/HLSL/", //Main include directory
			L"-I Resources/Shaders/",
			L"-D", L"__HLSL__",
			L"-D", L"HLSL",
		};

		const auto& globalMacros = Renderer::GetGlobalShaderMacros();
		for (const auto& [name, value] : globalMacros)
		{
			arguments.emplace_back(L"-D");
			arguments.push_back(nullptr);
			std::string def;
			if (value.size())
				def = fmt::format("{}={}", name, value);
			else
				def = name;

			wchar_t* def_buffer = new wchar_t[def.size() + 1];
			mbstowcs(def_buffer, def.c_str(), def.size());
			def_buffer[def.size()] = 0;
			arguments[arguments.size() - 1] = def_buffer;
		}

		for (const auto& [name, value] : m_Shader->m_PreDefines)
		{
			arguments.emplace_back(L"-D");

			std::wstring def;
			if (!value.empty())
				def = name + L"=" + value;
			else
				def = name;

			wchar_t* def_buffer = new wchar_t[def.size() + 1];
			std::memcpy(def_buffer, def.data(), (def.size() + 1) * sizeof(wchar_t));

			arguments.emplace_back(def_buffer);
		}

		if (!DxcInstances::Compiler)
		{
#ifdef BEY_PLATFORM_WINDOWS
			DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&DxcInstances::Compiler));
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&DxcInstances::Utils));
#endif
		}

		for (auto& [stage, shaderSource] : shaderSources)
		{
#ifdef BEY_PLATFORM_WINDOWS
			IDxcBlobEncoding* pSource;
			DxcInstances::Utils->CreateBlob(shaderSource.c_str(), (uint32_t)shaderSource.size(), CP_UTF8, &pSource);

			DxcBuffer sourceBuffer;
			sourceBuffer.Ptr = pSource->GetBufferPointer();
			sourceBuffer.Size = pSource->GetBufferSize();
			sourceBuffer.Encoding = 0;

			m_CurrentIncluder = std::make_unique<HlslIncluder>();
			IDxcResult* pCompileResult;
			HRESULT err = DxcInstances::Compiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), m_CurrentIncluder.get(), IID_PPV_ARGS(&pCompileResult));

			// Error Handling
			eastl::string error;
			const bool failed = FAILED(err);
			if (failed)
				error = fmt::eastl_format("Failed to pre-process, Error: {}\n", err);
			IDxcBlobEncoding* pErrors = nullptr;
			pCompileResult->GetErrorBuffer(&pErrors);
			if (pErrors->GetBufferPointer() && pErrors->GetBufferSize())
				error.append(fmt::eastl_format("{}\nWhile pre-processing shader file: {} \nAt stage: {}", (char*)pErrors->GetBufferPointer(), m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage)));

			if (error.empty())
			{
				// Successful compilation
				IDxcBlob* pResult;
				pCompileResult->GetResult(&pResult);

				const size_t size = pResult->GetBufferSize();
				shaderSource = (const char*)pResult->GetBufferPointer();
				pResult->Release();
			}
			else
			{
				BEY_CORE_ERROR_TAG("Renderer", error);
			}

			m_StagesMetadata[stage].HashValue = Hash::GenerateFNVHash(shaderSource.c_str());
			m_StagesMetadata[stage].Headers = std::move(m_CurrentIncluder->GetIncludeData());

			m_AcknowledgedMacros.merge(m_CurrentIncluder->GetParsedSpecialMacros());
#else
			m_StagesMetadata[stage] = StageData{};
#endif
		}
		return shaderSources;
	}

	eastl::string VulkanShaderCompiler::Compile(std::vector<uint32_t>& outputBinary, const VkShaderStageFlagBits stage, CompilationOptions options) const
	{
		const std::string& stageSource = m_ShaderSource.at(stage);

		if (m_Language == ShaderUtils::SourceLang::GLSL)
		{
			static shaderc::Compiler compiler;
			shaderc::CompileOptions shaderCOptions;
			shaderCOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			shaderCOptions.SetTargetSpirv(shaderc_spirv_version_1_5);
			shaderCOptions.SetWarningsAsErrors();
			if (options.GenerateDebugInfo)
				shaderCOptions.SetGenerateDebugInfo();

			if (options.Optimize)
				shaderCOptions.SetOptimizationLevel(shaderc_optimization_level_performance);

			// Compile shader
			const shaderc::SpvCompilationResult shaderModule = compiler.CompileGlslToSpv(stageSource.c_str(), ShaderUtils::ShaderStageToShaderC(stage), m_ShaderSourcePath.string().c_str(), shaderCOptions);

			if (shaderModule.GetCompilationStatus() != shaderc_compilation_status_success)
				return fmt::eastl_format("{}While compiling shader file: {} \nAt stage: {}", shaderModule.GetErrorMessage(), m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));

			outputBinary = std::vector<uint32_t>(shaderModule.begin(), shaderModule.end());
			return {}; // Success
		}
		else if (m_Language == ShaderUtils::SourceLang::HLSL)
		{
#ifdef BEY_PLATFORM_WINDOWS

			std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
			std::string entryPoint = converter.to_bytes(m_Shader->m_EntryPoint);
			const auto extension = ShaderUtils::ShaderStageCachedFileExtension(stage, options.GenerateDebugInfo);
			const std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

			auto path = cacheDirectory / (m_ShaderSourcePath.filename().stem().string() + "__" + entryPoint + m_ShaderSourcePath.extension().string() + extension + ".pdb");
			std::wstring cachedFilePath = path.wstring();

			std::wstring buffer = m_ShaderSourcePath.wstring();
			bool isSER = cachedFilePath.find(L"Pathtracing") != eastl::string::npos && (stage & (VK_SHADER_STAGE_CALLABLE_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
				VK_SHADER_STAGE_CLOSEST_HIT_BIT_NV | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_RAYGEN_BIT_KHR));
			std::vector<const wchar_t*> arguments{ buffer.c_str(), L"-E", m_Shader->m_EntryPoint.c_str(), L"-T", isSER ? L"lib_6_3" : ShaderUtils::HLSLShaderProfile(stage), L"-spirv", L"-fspv-target-env=vulkan1.3",
				L"-HV 2021",
				DXC_ARG_PACK_MATRIX_COLUMN_MAJOR, DXC_ARG_WARNINGS_ARE_ERRORS,
				L"-DENABLE_SPIRV_CODEGEN=ON",
				//L"-fspv-extension=SPV_NV_compute_shader_derivatives",
				//L"-fspv-extension=SPV_EXT_descriptor_indexing",
				//L"-fvk-use-gl-layout",
				//L"-line-directive",
				//DXC_ARG_DEBUG_NAME_FOR_SOURCE,
				//DXC_ARG_DEBUG_NAME_FOR_BINARY,
				//L"-fvk-use-scalar-layout",
				// TODO: L"-fspv-reflect" causes a validation error about SPV_GOOGLE_hlsl_functionality1
				// Without this argument, not much info will be in Nsight.
				//L"-fspv-reflect",
				L"-O3",
				//L"-fspv-debug=vulkan-with-source",

				//L"Fd",
				//cachedFilePath.c_str(),

			};

			if (options.GenerateDebugInfo)
			{
				arguments.emplace_back(L"-Qembed_debug");
				arguments.emplace_back(DXC_ARG_DEBUG);
			}

			if (stage & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_GEOMETRY_BIT))
				arguments.push_back(L"-fvk-invert-y");

			IDxcBlobEncoding* pSource;
			DxcInstances::Utils->CreateBlob(stageSource.c_str(), (uint32_t)stageSource.size(), CP_UTF8, &pSource);

			DxcBuffer sourceBuffer;
			sourceBuffer.Ptr = pSource->GetBufferPointer();
			sourceBuffer.Size = pSource->GetBufferSize();
			sourceBuffer.Encoding = 0;

			IDxcResult* pCompileResult;
			eastl::string error;

			HRESULT err = DxcInstances::Compiler->Compile(&sourceBuffer, arguments.data(), (uint32_t)arguments.size(), m_CurrentIncluder.get(), IID_PPV_ARGS(&pCompileResult));
			m_CurrentIncluder.reset();
			// Error Handling
			const bool failed = FAILED(err);
			if (failed)
				error = fmt::eastl_format("Failed to compile, Error: {}\n", err);
			IDxcBlobUtf8* pErrors;
			pCompileResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), NULL);

			IDxcBlob* pPdb = nullptr;
			pCompileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPdb), NULL);

			//if (pPdb && pPdb->GetBufferSize())
			{
				//
				// Save pdb.
				//
				IDxcBlob* pPDB = nullptr;
				IDxcBlobUtf16* pPDBName = nullptr;
				if (SUCCEEDED(pCompileResult->GetOutput(DXC_OUT_PDB, IID_PPV_ARGS(&pPDB), &pPDBName)))
				{
					FILE* fp = NULL;

					// Note that if you don't specify -Fd, a pdb name will be automatically generated.
					// Use this file name to save the pdb so that PIX can find it quickly.
					_wfopen_s(&fp, pPDBName->GetStringPointer(), L"wb");
					fwrite(pPDB->GetBufferPointer(), pPDB->GetBufferSize(), 1, fp);
					fclose(fp);
				}
			}

			if (pErrors && pErrors->GetStringLength() > 0)
				error.append(fmt::eastl_format("{}\nWhile compiling shader file: {} \nAt stage: {}", (char*)pErrors->GetBufferPointer(), m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage)));

			if (error.empty())
			{
				// Successful compilation
				IDxcBlob* pResult;
				pCompileResult->GetResult(&pResult);

				const size_t size = pResult->GetBufferSize();
				outputBinary.resize(size / sizeof(uint32_t));
				std::memcpy(outputBinary.data(), pResult->GetBufferPointer(), size);
				pResult->Release();
			}
			pCompileResult->Release();
			pSource->Release();

			return error;
#elif defined(BEY_PLATFORM_LINUX)
			// NOTE: This is *atrocious* but dxc's integration refuses to process builtin HLSL without ICE'ing
			//				from the integration.

			char tempfileName[] = "beyond-hlsl-XXXXXX.spv";
			int outfile = mkstemps(tempfileName, 4);

			eastl::string dxc = fmt::format("{}/bin/dxc", FileSystem::GetEnvironmentVariable("VULKAN_SDK"));
			eastl::string sourcePath = m_ShaderSourcePath.string();

			std::vector<const char*> exec{
				dxc.c_str(),
				sourcePath.c_str(),

				"-E", "main",
				"-T", ShaderUtils::HLSLShaderProfile(stage),
				"-spirv",
				"-fspv-target-env=vulkan1.2",
				"-Zpc",
				"-WX",

				"-I", "Resources/Shaders/Include/Common",
				"-I", "Resources/Shaders/Include/HLSL",

				"-Fo", tempfileName
			};

			if (options.GenerateDebugInfo)
			{
				exec.push_back("-Qembed_debug");
				exec.push_back("-Zi");
			}

			if (stage & (VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_GEOMETRY_BIT))
				exec.push_back("-fvk-invert-y");

			exec.push_back(NULL);

			// TODO: Error handling
			pid_t pid;
			posix_spawnattr_t attr;
			posix_spawnattr_init(&attr);

			eastl::string ld_lib_path = fmt::format("LD_LIBRARY_PATH={}", getenv("LD_LIBRARY_PATH"));
			char* env[] = { ld_lib_path.data(), NULL };
			if (posix_spawn(&pid, exec[0], NULL, &attr, (char**)exec.data(), env))
			{
				return fmt::format("Could not execute `dxc` for shader compilation", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));
			}
			int status;
			waitpid(pid, &status, 0);

			if (WEXITSTATUS(status))
			{
				return fmt::format("Compilation failed\nWhile compiling shader file: {} \nAt stage: {}", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));
			}

			off_t size = lseek(outfile, 0, SEEK_END);
			lseek(outfile, 0, SEEK_SET);
			outputBinary.resize(size / sizeof(uint32_t));
			read(outfile, outputBinary.data(), size);
			close(outfile);
			unlink(tempfileName);

			return {};
#endif
		}
		return "Unknown language!";
	}

	Ref<VulkanShader> VulkanShaderCompiler::Compile(const RootSignature rootSignature, const std::filesystem::path& shaderSourcePath, bool forceCompile, bool disableOptimization, bool external, const std::wstring& entryPoint, const std::wstring& targetProfile, const
													std::vector<std::pair<std::wstring, std::wstring>>& defines)
	{
		// Set name
		std::string path = shaderSourcePath.string();
		size_t found = path.find_last_of("/\\");
		std::string name = found != eastl::string::npos ? path.substr(found + 1) : path;
		found = name.find_last_of('.');
		name = found != eastl::string::npos ? name.substr(0, found) : name;

		Ref<VulkanShader> shader = Ref<VulkanShader>::Create();
		shader->m_AssetPath = shaderSourcePath;
		shader->m_Name = name;
		shader->m_DisableOptimization = disableOptimization;
		shader->m_EntryPoint = entryPoint;
		shader->m_PreDefines = defines;
		shader->m_ExternalShader = external;
		shader->m_TargetProfile = targetProfile;
		shader->m_RootSignature = rootSignature;
		shader->m_Hash = Hash::GenerateFNVHash(shader->m_AssetPath.string());

		Ref<VulkanShaderCompiler> compiler = Ref<VulkanShaderCompiler>::Create(shader, shaderSourcePath, disableOptimization);
		compiler->Reload(forceCompile);

		shader->Release();
		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);

		shader->CreateDescriptors();

		Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), shader);
		Renderer::OnShaderReloaded(shader->GetHash());
		return shader;
	}

	Ref<VulkanShader> VulkanShaderCompiler::Compile(const RootSignature rootSignature, const std::filesystem::path& shaderSourcePath, bool forceCompile, bool disableOptimization)
	{
		// Set name
		std::string path = shaderSourcePath.string();
		size_t found = path.find_last_of("/\\");
		std::string name = found != std::string::npos ? path.substr(found + 1) : path;
		found = name.find_last_of('.');
		name = found != std::string::npos ? name.substr(0, found) : name;

		Ref<VulkanShader> shader = Ref<VulkanShader>::Create();
		shader->m_AssetPath = shaderSourcePath;
		shader->m_Name = name;
		shader->m_DisableOptimization = disableOptimization;
		shader->m_RootSignature = rootSignature;
		shader->m_Hash = Hash::GenerateFNVHash(shader->m_AssetPath.string());

		Ref<VulkanShaderCompiler> compiler = Ref<VulkanShaderCompiler>::Create(shader, shaderSourcePath, disableOptimization);
		compiler->Reload(forceCompile);

		shader->Release();
		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), shader);
		Renderer::OnShaderReloaded(shader->GetHash());
		return shader;
	}

	bool VulkanShaderCompiler::TryRecompile(Ref<VulkanShader> shader)
	{
		Ref<VulkanShaderCompiler> compiler = Ref<VulkanShaderCompiler>::Create(shader, shader->m_AssetPath, shader->m_DisableOptimization);
		bool compileSucceeded = compiler->Reload(true);
		if (!compileSucceeded)
			return false;

		shader->Release();
		shader->LoadAndCreateShaders(compiler->GetSPIRVData());
		shader->SetReflectionData(compiler->m_ReflectionData);
		shader->CreateDescriptors();

		Renderer::AcknowledgeParsedGlobalMacros(compiler->GetAcknowledgedMacros(), shader);
		Renderer::OnShaderReloaded(shader->GetHash());

		return true;
	}

	bool VulkanShaderCompiler::CompileOrGetVulkanBinaries(std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputDebugBinary, std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& outputBinary, const VkShaderStageFlagBits changedStages, const bool forceCompile)
	{
		for (const auto stage : m_ShaderSource | std::views::keys)
		{
			if (!CompileOrGetVulkanBinary(stage, outputDebugBinary[stage], true, changedStages, forceCompile))
				return false;
			if (!CompileOrGetVulkanBinary(stage, outputBinary[stage], false, changedStages, forceCompile))
				return false;
		}
		return true;
	}


	bool VulkanShaderCompiler::CompileOrGetVulkanBinary(VkShaderStageFlagBits stage, std::vector<uint32_t>& outputBinary, bool debug, VkShaderStageFlagBits changedStages, bool forceCompile)
	{
		const std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		// Compile shader with debug info so we can reflect
		const auto extension = ShaderUtils::ShaderStageCachedFileExtension(stage, debug);
		if (!forceCompile && stage & ~changedStages) // Per-stage cache is found and is unchanged 
		{
			TryGetVulkanCachedBinary(cacheDirectory, extension, outputBinary);
		}

		if (outputBinary.empty())
		{
			CompilationOptions options;
			if (debug)
			{
				options.GenerateDebugInfo = true;
				options.Optimize = false;
			}
			else
			{
				options.GenerateDebugInfo = true;
				// Disable optimization for compute shaders because of shaderc internal error
				options.Optimize = !m_DisableOptimization;// && stage != VK_SHADER_STAGE_COMPUTE_BIT;
			}

			if (eastl::string error = Compile(outputBinary, stage, options); error.size())
			{
				BEY_CORE_ERROR_TAG("Renderer", "{}", error);
				TryGetVulkanCachedBinary(cacheDirectory, extension, outputBinary);
				if (outputBinary.empty())
				{
					BEY_CONSOLE_LOG_ERROR("Failed to compile shader and couldn't find a cached version.");
				}
				else
				{
					BEY_CONSOLE_LOG_ERROR("Failed to compile {}:{} so a cached version was loaded instead.", m_ShaderSourcePath.string(), ShaderUtils::ShaderStageToString(stage));
					BEY_CONSOLE_LOG_ERROR(error);
					BEY_CORE_VERIFY_MESSAGE_INTERNAL("Shader Compilation Error: {}", error);
#if 1
					if (GImGui) // Guaranteed to be null before first ImGui frame
					{
						ImGuiWindow* logWindow = ImGui::FindWindowByName("Log");
						ImGui::FocusWindow(logWindow);
					}
#endif
				}
				return false;
			}
			else // Compile success
			{
				//#define OPT_SHADERS
#ifdef OPT_SHADERS
				if (options.Optimize)
				{
					/// Optimize SPIR - V
					spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_3);
					optimizer.RegisterPass(spvtools::CreateRemoveUnusedInterfaceVariablesPass());
					optimizer.RegisterPass(spvtools::CreateDeadVariableEliminationPass());
					optimizer.RegisterPass(spvtools::CreateRedundancyEliminationPass());
					optimizer.RegisterPass(spvtools::CreateReduceLoadSizePass());
					optimizer.RegisterPass(spvtools::CreateRedundantLineInfoElimPass());
					std::vector<uint32_t> optimized;
					optimizer.SetTargetEnv(SPV_ENV_VULKAN_1_3);
					spvtools::MessageConsumer consumer([](spv_message_level_t /* level */, const char* /* source */,
						const spv_position_t& /* position */, const char* message)
					{
						BEY_CORE_ERROR(message);
						BEY_CORE_ASSERT(false);
					});
					optimizer.SetMessageConsumer(consumer);
					optimizer.Run(outputBinary.data(), outputBinary.size(), &optimized);

					outputBinary = optimized;
				}
#endif

				std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
				std::string entryPoint = converter.to_bytes(m_Shader->m_EntryPoint);

				auto path = cacheDirectory / (m_ShaderSourcePath.filename().stem().string() + "__" + entryPoint + m_ShaderSourcePath.extension().string() + extension);
				std::string cachedFilePath = path.string();

				FILE* f = fopen(cachedFilePath.c_str(), "wb");
				if (!f)
					BEY_CORE_ERROR("Failed to cache shader binary!");
				else
				{
					fwrite(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
					fclose(f);
				}
			}
		}

		return true;
	}

	void VulkanShaderCompiler::ClearReflectionData()
	{
		m_ReflectionData.ShaderDescriptorSets.clear();
		m_ReflectionData.Resources.clear();
		m_ReflectionData.ConstantBuffers.clear();
		m_ReflectionData.PushConstantRanges.clear();
	}

	void VulkanShaderCompiler::TryGetVulkanCachedBinary(const std::filesystem::path& cacheDirectory, const std::string& extension, std::vector<uint32_t>& outputBinary) const
	{
		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		std::string entryPoint = converter.to_bytes(m_Shader->m_EntryPoint);
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().stem().string() + "__" + entryPoint + m_ShaderSourcePath.extension().string() + extension);
		const std::string cachedFilePath = path.string();

		FILE* f = fopen(cachedFilePath.data(), "rb");
		if (!f)
			return;

		fseek(f, 0, SEEK_END);
		uint64_t size = ftell(f);
		fseek(f, 0, SEEK_SET);
		outputBinary = std::vector<uint32_t>(size / sizeof(uint32_t));
		fread(outputBinary.data(), sizeof(uint32_t), outputBinary.size(), f);
		fclose(f);
	}

	bool VulkanShaderCompiler::TryReadCachedReflectionData()
	{
		struct ReflectionFileHeader
		{
			char Header[4] = { 'H','Z','S','R' };
		} header;

		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		std::string entryPoint = converter.to_bytes(m_Shader->m_EntryPoint);

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().stem().string() + "__" + entryPoint + m_ShaderSourcePath.extension().string() + ".cached_vulkan.refl");
		FileStreamReader serializer(path);
		if (!serializer)
			return false;

		serializer.ReadRaw(header);

		bool validHeader = memcmp(&header, "HZSR", 4) == 0;
		BEY_CORE_VERIFY(validHeader);
		if (!validHeader)
			return false;

		ClearReflectionData();

		uint32_t shaderDescriptorSetCount;
		serializer.ReadRaw<uint32_t>(shaderDescriptorSetCount);

		for (uint32_t i = 0; i < shaderDescriptorSetCount; i++)
		{
			auto& descriptorSet = m_ReflectionData.ShaderDescriptorSets.emplace_back();
			serializer.ReadMap(descriptorSet.UniformBuffers);
			serializer.ReadMap(descriptorSet.StorageBuffers);
			serializer.ReadMap(descriptorSet.ImageSamplers);
			serializer.ReadMap(descriptorSet.StorageImages);
			serializer.ReadMap(descriptorSet.SeparateTextures);
			serializer.ReadMap(descriptorSet.SeparateSamplers);
			serializer.ReadMap(descriptorSet.AccelerationStructures);
			serializer.ReadMap(descriptorSet.WriteDescriptorSets);
			serializer.ReadArray(descriptorSet.Bindings);
		}

		serializer.ReadMap(m_ReflectionData.Resources);
		serializer.ReadMap(m_ReflectionData.ConstantBuffers);
		serializer.ReadArray(m_ReflectionData.PushConstantRanges);

		return true;
	}

	void VulkanShaderCompiler::SerializeReflectionData()
	{
		struct ReflectionFileHeader
		{
			char Header[4] = { 'H','Z','S','R' };
		} header;

		std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
		std::string entryPoint = converter.to_bytes(m_Shader->m_EntryPoint);

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();
		const auto path = cacheDirectory / (m_ShaderSourcePath.filename().stem().string() + "__" + entryPoint + m_ShaderSourcePath.extension().string() + ".cached_vulkan.refl");
		FileStreamWriter serializer(path);
		serializer.WriteRaw(header);
		SerializeReflectionData(&serializer);
	}

	void VulkanShaderCompiler::SerializeReflectionData(StreamWriter* serializer)
	{
		serializer->WriteRaw<uint32_t>((uint32_t)m_ReflectionData.ShaderDescriptorSets.size());
		for (const auto& descriptorSet : m_ReflectionData.ShaderDescriptorSets)
		{
			serializer->WriteMap(descriptorSet.UniformBuffers);
			serializer->WriteMap(descriptorSet.StorageBuffers);
			serializer->WriteMap(descriptorSet.ImageSamplers);
			serializer->WriteMap(descriptorSet.StorageImages);
			serializer->WriteMap(descriptorSet.SeparateTextures);
			serializer->WriteMap(descriptorSet.SeparateSamplers);
			serializer->WriteMap(descriptorSet.AccelerationStructures);
			serializer->WriteMap(descriptorSet.WriteDescriptorSets);
			serializer->WriteArray(descriptorSet.Bindings, true);
		}

		serializer->WriteMap(m_ReflectionData.Resources);
		serializer->WriteMap(m_ReflectionData.ConstantBuffers);
		serializer->WriteArray(m_ReflectionData.PushConstantRanges);
	}

	void VulkanShaderCompiler::ReflectAllShaderStages(const std::map<VkShaderStageFlagBits, std::vector<uint32_t>>& shaderData)
	{
		ClearReflectionData();

		for (auto [stage, data] : shaderData)
		{
			Reflect(stage, data);
		}
		ClearUniformBuffers();
	}

	void VulkanShaderCompiler::Reflect(VkShaderStageFlagBits shaderStage, const std::vector<uint32_t>& shaderData)
	{
		VkDevice device = VulkanContext::GetCurrentDevice()->GetVulkanDevice();

		BEY_CORE_TRACE_TAG("Renderer", "=========================================");
		BEY_CORE_TRACE_TAG("Renderer", " Vulkan Shader Reflection. Stage: {}", ShaderUtils::ShaderStageToString(shaderStage));
		BEY_CORE_TRACE_TAG("Renderer", "=========================================");

#if 0
		FILE* f = fopen("asdf.spv", "wb");
		fwrite(shaderData.data(), 4, shaderData.size(), f);
		fclose(f);
#endif
		spirv_cross::Compiler compiler(shaderData);
		auto resources = compiler.get_shader_resources();

		if (!resources.uniform_buffers.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Uniform Buffers:");
		for (const auto& resource : resources.uniform_buffers)
		{
			const auto& activeBuffers = compiler.get_active_buffer_ranges(resource.id);
			// Discard unused buffers from headers
			if (activeBuffers.size())
			{
				const auto& name = compiler.get_name(resource.id);

				//const auto& name = resource.name;
				auto& bufferType = compiler.get_type(resource.base_type_id);
				int memberCount = (uint32_t)bufferType.member_types.size();
				uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
				uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
				uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);
				uint32_t arraySize = Utils::GetReflectedArraySize(bufferType);
				if (arraySize == 0)
					arraySize = 1;
				if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
					m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

				ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
				if (!s_UniformBuffers[descriptorSet].contains(binding))
				{
					ShaderResource::UniformBuffer uniformBuffer;
					uniformBuffer.BindingPoint = binding;
					uniformBuffer.ArraySize = arraySize;
					uniformBuffer.Size = size;
					uniformBuffer.Name = eastl::string(name.c_str(), name.size());
					uniformBuffer.ShaderStage = (VkShaderStageFlagBits)(uniformBuffer.ShaderStage | shaderStage);
					s_UniformBuffers.at(descriptorSet)[binding] = uniformBuffer;
				}
				else
				{
					ShaderResource::UniformBuffer& uniformBuffer = s_UniformBuffers.at(descriptorSet).at(binding);
					if (size > uniformBuffer.Size)
						uniformBuffer.Size = size;
					uniformBuffer.ShaderStage = (VkShaderStageFlagBits)(uniformBuffer.ShaderStage | shaderStage);
				}
				shaderDescriptorSet.UniformBuffers[binding] = s_UniformBuffers.at(descriptorSet).at(binding);

				BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
				BEY_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
				BEY_CORE_TRACE_TAG("Renderer", "  Size: {0}", size);
				BEY_CORE_TRACE_TAG("Renderer", "-------------------");
			}
		}

		if (!resources.storage_buffers.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Storage Buffers:");
		for (const auto& resource : resources.storage_buffers)
		{
			auto reflectStorageBuffers = [&]()
			{
				{
					//const auto& name = resource.name;
					const auto& name = compiler.get_name(resource.id);

					auto& bufferType = compiler.get_type(resource.base_type_id);
					uint32_t memberCount = (uint32_t)bufferType.member_types.size();
					uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
					uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
					uint32_t size = (uint32_t)compiler.get_declared_struct_size(bufferType);
					uint32_t arraySize = Utils::GetReflectedArraySize(bufferType);
					if (arraySize == 0)
						arraySize = 1;

					if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
						m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

					ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
					if (!s_StorageBuffers[descriptorSet].contains(binding))
					{
						ShaderResource::StorageBuffer storageBuffer;
						storageBuffer.BindingPoint = binding;
						storageBuffer.ArraySize = arraySize;
						storageBuffer.Size = size;
						storageBuffer.Name = eastl::string(name.c_str(), name.size());
						storageBuffer.ShaderStage = (VkShaderStageFlagBits)(storageBuffer.ShaderStage | shaderStage);
						s_StorageBuffers.at(descriptorSet)[binding] = storageBuffer;
					}
					else
					{
						ShaderResource::StorageBuffer& storageBuffer = s_StorageBuffers.at(descriptorSet).at(binding);
						if (size > storageBuffer.Size)
							storageBuffer.Size = size;
						storageBuffer.ShaderStage = (VkShaderStageFlagBits)(storageBuffer.ShaderStage | shaderStage);
					}

					shaderDescriptorSet.StorageBuffers[binding] = s_StorageBuffers.at(descriptorSet).at(binding);

					BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
					BEY_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
					BEY_CORE_TRACE_TAG("Renderer", "  Size: {0}", size);
					BEY_CORE_TRACE_TAG("Renderer", "-------------------");
				}
			};

			if (resource.name == "ByteAddrBuffer")
			{
				reflectStorageBuffers();
			}
			else
			{
				const auto& activeBuffers = compiler.get_active_buffer_ranges(resource.id);
				if (activeBuffers.size())
					reflectStorageBuffers();
			}

		}

		if (!resources.acceleration_structures.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Accleration Structures:");
		for (const auto& resource : resources.acceleration_structures)
		{
			const auto& name = eastl::string(resource.name.c_str(), resource.name.size());
			auto& type = compiler.get_type(resource.base_type_id);
			uint32_t memberCount = (uint32_t)type.member_types.size();
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t arraySize = Utils::GetReflectedArraySize(type);
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			if (!s_AccelerationStructures[descriptorSet].contains(binding))
			{
				ShaderResource::AccelerationStructure accelerationStructure;
				accelerationStructure.BindingPoint = binding;
				accelerationStructure.DescriptorSet = descriptorSet;
				accelerationStructure.Name = name;
				accelerationStructure.ArraySize = arraySize;
				accelerationStructure.ShaderStage = (VkShaderStageFlagBits)(accelerationStructure.ShaderStage | shaderStage);
				s_AccelerationStructures.at(descriptorSet)[binding] = accelerationStructure;
			}
			else
			{
				ShaderResource::AccelerationStructure& accelerationStructure = s_AccelerationStructures.at(descriptorSet).at(binding);
				accelerationStructure.ShaderStage = (VkShaderStageFlagBits)(accelerationStructure.ShaderStage | shaderStage);
			}

			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, RenderPassInputType::AccelerationStructure, descriptorSet, binding, arraySize);

			shaderDescriptorSet.AccelerationStructures[binding] = s_AccelerationStructures.at(descriptorSet).at(binding);

			BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
			//BEY_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
			//BEY_CORE_TRACE_TAG("Renderer", "  Size: {0}", size);
			//BEY_CORE_TRACE_TAG("Renderer", "-------------------");
		}

		auto alignTo16 = [](uint32_t value)
		{
			return ((value + 15) / 16) * 16;
		};

		if (!resources.push_constant_buffers.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Push Constant Buffers:");
		for (const auto& resource : resources.push_constant_buffers)
		{
			const auto& bufferName = eastl::string(resource.name.c_str(), resource.name.size());
			auto& bufferType = compiler.get_type(resource.base_type_id);
			auto bufferSize = alignTo16((uint32_t)compiler.get_declared_struct_size(bufferType));
			uint32_t memberCount = uint32_t(bufferType.member_types.size());
			uint32_t bufferOffset = 0;
			bool bufferAlreadyReflected = m_ReflectionData.ConstantBuffers.contains(bufferName);
			if (!m_ReflectionData.PushConstantRanges.empty() && !bufferAlreadyReflected)
				bufferOffset = m_ReflectionData.PushConstantRanges.back().Offset + m_ReflectionData.PushConstantRanges.back().Size;

			auto& pushConstantRange = m_ReflectionData.PushConstantRanges.emplace_back();
			*(uint32_t*)&pushConstantRange.ShaderStage |= shaderStage;
			pushConstantRange.Size = bufferSize - bufferOffset;
			pushConstantRange.Offset = bufferOffset;

			// Skip empty push constant buffers - these are for the renderer only
			if (bufferName.empty() || bufferName == "u_Renderer")
				continue;

			ShaderBuffer& buffer = m_ReflectionData.ConstantBuffers[bufferName];
			buffer.Name = bufferName;
			buffer.Size = bufferSize - bufferOffset;

			BEY_CORE_TRACE_TAG("Renderer", "  Name: {0}", bufferName);
			BEY_CORE_TRACE_TAG("Renderer", "  Size: {0}", buffer.Size);
			BEY_CORE_TRACE_TAG("Renderer", "  Member Count: {0}", memberCount);
			BEY_CORE_TRACE_TAG("Renderer", "  Members:");

			for (uint32_t i = 0; i < memberCount; i++)
			{
				auto type = compiler.get_type(bufferType.member_types[i]);
				const auto& memberName = compiler.get_member_name(bufferType.self, i);
				auto size = (uint32_t)compiler.get_declared_struct_member_size(bufferType, i);
				auto offset = compiler.type_struct_member_offset(bufferType, i) - bufferOffset;

				eastl::string uniformName = fmt::eastl_format("{}.{}", bufferName, memberName);
				buffer.Uniforms[uniformName] = ShaderUniform(uniformName, Utils::SPIRTypeToShaderUniformType(type), size, offset);
				BEY_CORE_TRACE_TAG("Renderer", "    {}", uniformName);

			}
			BEY_CORE_TRACE_TAG("Renderer", "-------------------");

		}

		if (!resources.sampled_images.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Sampled Images:");
		for (const auto& resource : resources.sampled_images)
		{
			const auto& name = eastl::string(resource.name.c_str(), resource.name.size());
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = Utils::NumDims(baseType.image.dim);
			uint32_t arraySize = Utils::GetReflectedArraySize(type);

			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.ImageSamplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = (VkShaderStageFlagBits)(imageSampler.ShaderStage | shaderStage);
			imageSampler.Dimension = dimension;
			imageSampler.ArraySize = arraySize;


			auto imageType = Utils::RenderPassInputTypeFromReflection(dimension, true);
			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, imageType, descriptorSet, binding, arraySize);


			BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
			//BEY_CORE_TRACE_TAG("Renderer", "-------------------");
		}

		if (!resources.separate_images.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Separate Images:");
		for (const auto& resource : resources.separate_images)
		{
			const auto& name = eastl::string(resource.name.c_str(), resource.name.size());
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = Utils::NumDims(baseType.image.dim);
			uint32_t arraySize = Utils::GetReflectedArraySize(type);

			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.SeparateTextures[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = (VkShaderStageFlagBits)(imageSampler.ShaderStage | shaderStage);
			imageSampler.Dimension = dimension;
			imageSampler.ArraySize = arraySize;

			auto imageType = Utils::RenderPassInputTypeFromReflection(dimension, type.image.sampled);
			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, imageType, descriptorSet, binding, arraySize);

			BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
			//BEY_CORE_TRACE_TAG("Renderer", "-------------------");
		}

		if (!resources.separate_samplers.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Separate Samplers:");
		for (const auto& resource : resources.separate_samplers)
		{
			const auto& name = eastl::string(resource.name.c_str(), resource.name.size());
			auto& baseType = compiler.get_type(resource.base_type_id);
			auto& type = compiler.get_type(resource.type_id);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			uint32_t dimension = Utils::NumDims(baseType.image.dim);
			uint32_t arraySize = Utils::GetReflectedArraySize(type);
			if (arraySize == 0)
				arraySize = 1;
			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.SeparateSamplers[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.ShaderStage = (VkShaderStageFlagBits)(imageSampler.ShaderStage | shaderStage);
			imageSampler.Dimension = dimension;
			imageSampler.ArraySize = arraySize;

			auto imageType = Utils::RenderPassInputTypeFromReflection(dimension, type.image.sampled);
			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, imageType, descriptorSet, binding, arraySize);

			BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
			//BEY_CORE_TRACE_TAG("Renderer", "-------------------");
		}

		if (!resources.storage_images.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Storage Images:");
		for (const auto& resource : resources.storage_images)
		{
			const auto& name = eastl::string(resource.name.c_str(), resource.name.size());
			const auto& type = compiler.get_type(resource.type_id);
			const uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			const uint32_t descriptorSet = compiler.get_decoration(resource.id, spv::DecorationDescriptorSet);
			const auto& baseType = compiler.get_type(resource.base_type_id);

			uint32_t dimension = Utils::NumDims(type.image.dim);
			uint32_t arraySize = Utils::GetReflectedArraySize(type);
			if (arraySize == 0)
				arraySize = 1;

			if (descriptorSet >= m_ReflectionData.ShaderDescriptorSets.size())
				m_ReflectionData.ShaderDescriptorSets.resize(descriptorSet + 1);

			ShaderResource::ShaderDescriptorSet& shaderDescriptorSet = m_ReflectionData.ShaderDescriptorSets[descriptorSet];
			auto& imageSampler = shaderDescriptorSet.StorageImages[binding];
			imageSampler.BindingPoint = binding;
			imageSampler.DescriptorSet = descriptorSet;
			imageSampler.Name = name;
			imageSampler.Dimension = dimension;
			imageSampler.ArraySize = arraySize;
			imageSampler.ShaderStage = (VkShaderStageFlagBits)(imageSampler.ShaderStage | shaderStage);

			auto imageType = Utils::RenderPassInputTypeFromReflection(dimension, type.image.sampled);
			m_ReflectionData.Resources[name] = ShaderResourceDeclaration(name, imageType, descriptorSet, binding, arraySize);

			BEY_CORE_TRACE_TAG("Renderer", "  {0} ({1}, {2})", name, descriptorSet, binding);
		}

		if (!m_AcknowledgedMacros.empty())
			BEY_CORE_TRACE_TAG("Renderer", "Special macros:");
		for (const auto& macro : m_AcknowledgedMacros)
		{
			BEY_CORE_TRACE_TAG("Renderer", "  {0}", macro);
		}

		if (!resources.storage_images.empty())
			BEY_CORE_TRACE_TAG("Renderer", "===========================");


	}


}
