#pragma once
//#include <compressonator.h>

namespace  Beyond {

	//struct CMP_CMIPS
	//{
	//	// User Configurable Print lines
	//	int  m_infolevel = 1;
	//	void PrintError(const char* Format, ...);
	//	void (*PrintLine)(char*) = nullptr;
	//	void Print(const char* Format, ...);

	//	CMP_MipLevel* GetMipLevel(const CMP_MipSet* pMipSet, CMP_INT nMipLevel, CMP_INT nFaceOrSlice = 0);

	//	int GetMaxMipLevels(CMP_INT nWidth, CMP_INT nHeight, CMP_INT nDepth);

	//	bool AllocateMipLevelTable(CMP_MipLevelTable** ppMipLevelTable,
	//							   CMP_INT             nMaxMipLevels,
	//							   CMP_TextureType     textureType,
	//							   CMP_INT             nDepth,
	//							   CMP_INT& nLevelsToAllocate);

	//	bool AllocateAllMipLevels(CMP_MipLevelTable* pMipLevelTable, CMP_TextureType /*textureType*/, CMP_INT nLevelsToAllocate);

	//	bool AllocateMipSet(CMP_MipSet* pMipSet,
	//						CMP_ChannelFormat   channelFormat,
	//						CMP_TextureDataType textureDataType,
	//						CMP_TextureType     textureType,
	//						CMP_INT             nWidth,
	//						CMP_INT             nHeight,
	//						CMP_INT             nDepth);

	//	bool AllocateMipLevelData(CMP_MipLevel* pMipLevel, CMP_INT nWidth, CMP_INT nHeight, CMP_ChannelFormat channelFormat, CMP_TextureDataType textureDataType);
	//	bool AllocateCompressedMipLevelData(CMP_MipLevel* pMipLevel, CMP_INT nWidth, CMP_INT nHeight, CMP_DWORD dwSize);

	//	void FreeMipSet(CMP_MipSet* pMipSet);            // Removes entire mipset
	//	void FreeMipLevelData(CMP_MipLevel* pMipLevel);  // removes a single miplevel generated by ...MipLevelData()

	//	bool AllocateCompressedDestBuffer(CMP_MipSet* SourceTexture, CMP_FORMAT format, CMP_MipSet* DestTexture);

	//	// Progress
	//	bool m_canceled = false;
	//	void (*SetProgressValue)(unsigned int, bool* canceled) = nullptr;
	//	void SetProgress(unsigned int value);
	//};

}