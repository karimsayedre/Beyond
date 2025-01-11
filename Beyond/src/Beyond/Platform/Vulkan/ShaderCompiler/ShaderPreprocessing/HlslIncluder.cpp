#include "pch.h"
#include "HlslIncluder.h"

#include "ShaderPreprocessor.h"

#include "Beyond/Core/Hash.h"
#include "Beyond/Utilities/StringUtils.h"

namespace Beyond
{
	HRESULT HlslIncluder::LoadSource(LPCWSTR pFilename, IDxcBlob** ppIncludeSource)
	{
		static IDxcUtils* pUtils = nullptr;
		if (!pUtils)
		{
			DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils));
			HRESULT result = pUtils->CreateDefaultIncludeHandler(&s_DefaultIncludeHandler);
			BEY_CORE_ASSERT(!FAILED(result), "Failed to create default include handler!");
		}

		const std::filesystem::path filePath = pFilename;
		auto& [source, sourceHash, stages, isGuarded] = m_HeaderCache[filePath.string()];

		if(source.empty())
		{
			source = Utils::ReadFileAndSkipBOM(filePath);
			if (source.empty())
			{
				// NOTE: No error logging because dxc tries multiple include
				// directories with the same file until it finds it.
				*ppIncludeSource = nullptr;
				return S_FALSE;
			}

			sourceHash = Hash::GenerateFNVHash(source);

			// Can clear "source" in case it has already been included in this stage.
			stages = ShaderPreprocessor::PreprocessHeader<ShaderUtils::SourceLang::HLSL>(source, isGuarded, m_ParsedSpecialMacros, m_includeData, filePath);
		}
		else if (isGuarded)
		{
			source.clear();
		}

		//TODO: Get real values for IncludeDepth and IsRelative?
		m_includeData.emplace(IncludeData{ filePath, 0, false, isGuarded, sourceHash, stages });

		IDxcBlobEncoding* pEncoding;
		pUtils->CreateBlob(source.data(), (uint32_t)source.size(), CP_UTF8, &pEncoding);

		*ppIncludeSource = pEncoding;
		return S_OK;
	}

}
