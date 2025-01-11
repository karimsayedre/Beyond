#include "pch.h"
//#include "Cmips.h"
//
////=====================================================================
//// Copyright 2008 (c), ATI Technologies Inc. All rights reserved.
//// Copyright 2022 (c), Advanced Micro Devices, Inc. All rights reserved.
////=====================================================================
////
//// Permission is hereby granted, free of charge, to any person obtaining a copy
//// of this software and associated documentation files(the "Software"), to deal
//// in the Software without restriction, including without limitation the rights
//// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
//// copies of the Software, and to permit persons to whom the Software is
//// furnished to do so, subject to the following conditions :
////
//// The above copyright notice and this permission notice shall be included in
//// all copies or substantial portions of the Software.
////
//// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//// THE SOFTWARE.
////
//
////#include "compressonator.h"
//
//
//#include <stdarg.h>
//#include <stdio.h>
//#include <assert.h>
//
//#ifndef CMP_MAX
//#define CMP_MAX(x, y) (((x) > (y)) ? (x) : (y))
//#endif
//
//#define CMP_PAD_BYTE(width, bpp) (((bpp) * (width) + 7) / 8)
//
//#define CMP_MIPS_FOURCC_DX10                    CMP_MAKEFOURCC('D', 'X', '1', '0')
//
//template <class T> const T& max(const T& a, const T& b)
//{
//	return (a < b) ? b : a;
//}
//
//namespace Beyond
//{
//
//
//
//
//	void (*PrintStatusLine)(char*) = NULL;
//
//	void PrintInfo(const char* Format, ...)
//	{
//		if (PrintStatusLine)
//		{
//			// define a pointer to save argument list
//			va_list args;
//			char    buff[1024];
//			// process the arguments into our debug buffer
//			va_start(args, Format);
//			vsnprintf(buff, 1024, Format, args);
//			va_end(args);
//			PrintStatusLine(buff);
//		}
//	}
//
//	void CMP_CMIPS::PrintError(const char* Format, ...)
//	{
//		char buff[1024];
//		// define a pointer to save argument list
//		va_list args;
//		// process the arguments into our debug buffer
//		va_start(args, Format);
//		vsnprintf(buff, 1024, Format, args);
//		va_end(args);
//
//		PrintInfo(buff);
//	}
//
//	void CMP_CMIPS::Print(const char* Format, ...)
//	{
//		if (!PrintLine)
//			return;
//
//		if (m_infolevel & 0x01)
//		{
//			char buff[1024];
//			// define a pointer to save argument list
//			va_list args;
//			// process the arguments into our debug buffer
//			va_start(args, Format);
//			vsnprintf(buff, 1024, Format, args);
//			va_end(args);
//
//			PrintLine(buff);
//		}
//	}
//
//	// Determines the active channels used for a given format
//	// See CMP_AnalysisData for more details on the bits set
//	CMP_UINT CMP_API CMP_getFormat_nChannels(CMP_FORMAT format)
//	{
//		CMP_UINT RGBAChannels;
//
//		switch (format)
//		{
//			case CMP_FORMAT_ATI1N:
//			case CMP_FORMAT_BC4:
//			case CMP_FORMAT_BC4_S:      // All channels are used and equal, Red is used as active channel
//				RGBAChannels = 0b0001;  // R
//				break;
//			case CMP_FORMAT_ATI2N_XY:
//			case CMP_FORMAT_BC5_S:
//			case CMP_FORMAT_BC5:        // Only Red & Green channels active
//				RGBAChannels = 0b0011;  // GR
//				break;
//			default:
//				RGBAChannels = 0b0111;  // BGR , alpha skipped user can set this after
//				break;
//		}
//
//		return RGBAChannels;
//	}
//
//	CMP_INT CMP_API CMP_CalcMinMipSize(CMP_INT nHeight, CMP_INT nWidth, CMP_INT MipsLevel)
//	{
//		while (MipsLevel > 1)
//		{
//			nWidth = CMP_MAX(nWidth >> 1, 1);
//			nHeight = CMP_MAX(nHeight >> 1, 1);
//			MipsLevel--;
//		}
//		return (nWidth);
//	}
//
//	CMP_INT CMP_API CMP_CalcMaxMipLevel(CMP_INT nHeight, CMP_INT nWidth, CMP_BOOL bForGPU)
//	{
//#define MAX_MIPLEVEL_SUPPORTED 12
//		CMP_INT MaxMipLevel = 1;
//		while (MaxMipLevel < MAX_MIPLEVEL_SUPPORTED && (nWidth > 1 || nHeight > 1))
//		{
//			nWidth = CMP_MAX(nWidth >> 1, 1);
//			nHeight = CMP_MAX(nHeight >> 1, 1);
//			if (bForGPU)
//			{
//				if ((nWidth % 4) || (nHeight % 4))
//					break;
//			}
//			MaxMipLevel++;
//		}
//
//		return MaxMipLevel;
//	}
//
//	CMP_ERROR CMP_API CMP_CreateCompressMipSet(CMP_MipSet* pMipSetCMP, CMP_MipSet* pMipSetSRC)
//	{
//		CMP_CMIPS CMips;
//
//		pMipSetCMP->m_Flags = MS_FLAG_Default;
//		pMipSetCMP->m_nHeight = pMipSetSRC->m_nHeight;
//		pMipSetCMP->m_nWidth = pMipSetSRC->m_nWidth;
//		pMipSetCMP->dwWidth = pMipSetSRC->dwWidth;
//		pMipSetCMP->dwHeight = pMipSetSRC->dwHeight;
//		pMipSetCMP->m_ChannelFormat = CF_Compressed;
//		pMipSetCMP->m_dwFourCC = CMP_MAKEFOURCC('D', 'X', '1', '0');
//		pMipSetCMP->m_nBlockHeight = 4;
//		pMipSetCMP->m_nBlockWidth = 4;
//		pMipSetCMP->m_nMaxMipLevels = pMipSetSRC->m_nMaxMipLevels;
//		pMipSetCMP->m_nMipLevels = 1;
//		pMipSetCMP->m_TextureType = pMipSetSRC->m_TextureType;
//		pMipSetCMP->m_nDepth = pMipSetSRC->m_nDepth;
//
//		if (!CMips.AllocateMipSet(
//			pMipSetCMP, CF_Compressed, TDT_ARGB, pMipSetCMP->m_TextureType, pMipSetSRC->m_nWidth, pMipSetSRC->m_nHeight, pMipSetCMP->m_nDepth))
//			return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
//
//		//----------------------------------------------------------------
//		// Access the data table for mip level 0 (full texture width and height)
//		//----------------------------------------------------------------
//
//		MipLevel* pOutMipLevel = CMips.GetMipLevel(pMipSetCMP, 0);
//
//		//-----------------------------------------------------------------------
//		// Calculate the target compressed block buffer size (4 bytes x 4 bytes)
//		//-----------------------------------------------------------------------
//		unsigned int Width = ((pMipSetSRC->m_nWidth + 3) / 4) * 4;
//		unsigned int Height = ((pMipSetSRC->m_nHeight + 3) / 4) * 4;
//		pMipSetCMP->dwDataSize = Width * Height;
//
//		//----------------------------------------------------------------
//		// Allocate memory for the mip level 0
//		//----------------------------------------------------------------
//		if (!CMips.AllocateCompressedMipLevelData(pOutMipLevel, pMipSetSRC->m_nWidth, pMipSetSRC->m_nHeight, pMipSetCMP->dwDataSize))
//		{
//			std::printf("Memory Error(1): allocating MIPSet compression level data buffer\n");
//			return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
//		}
//
//		pMipSetCMP->pData = pOutMipLevel->m_pbData;
//
//		return CMP_OK;
//	}
//
//	CMP_ERROR CMP_API CMP_CreateMipSet(CMP_MipSet* pMipSet, CMP_INT nWidth, CMP_INT nHeight, CMP_INT nDepth, ChannelFormat channelFormat, TextureType textureType)
//	{
//		CMP_CMIPS CMips;
//
//		pMipSet->m_Flags = MS_FLAG_Default;
//		pMipSet->m_nHeight = nHeight;
//		pMipSet->m_nWidth = nWidth;
//		pMipSet->dwWidth = 0;
//		pMipSet->dwHeight = 0;
//		pMipSet->m_ChannelFormat = channelFormat;
//		pMipSet->m_dwFourCC = 0;
//		pMipSet->m_nBlockHeight = 4;
//		pMipSet->m_nBlockWidth = 4;
//		pMipSet->m_nMaxMipLevels = CMP_CalcMaxMipLevel(nHeight, nWidth, false);
//		pMipSet->m_nMipLevels = 1;
//		pMipSet->m_TextureType = textureType;
//		pMipSet->m_nDepth = nDepth;
//
//		if (!CMips.AllocateMipSet(pMipSet, channelFormat, TDT_ARGB, pMipSet->m_TextureType, nWidth, nHeight, nDepth))
//			return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
//
//		//----------------------------------------------------------------
//		// Access the data table for mip level 0 (full texture width and height)
//		//----------------------------------------------------------------
//
//		MipLevel* pOutMipLevel = CMips.GetMipLevel(pMipSet, 0);
//
//		//-----------------------------------------------------------------------
//		// Calculate the target compressed block buffer size (4 bytes x 4 bytes)
//		//-----------------------------------------------------------------------
//		CMP_INT bytesPerChannel; // use m_ChannelFormat for this 
//
//		switch (channelFormat)
//		{
//			case CF_16bit:
//			case CF_Float16:
//				bytesPerChannel = 8;
//				break;
//			case CF_32bit:
//			case CF_Float32:
//				bytesPerChannel = 16;
//				break;
//			case CF_1010102:
//			case CF_2101010:
//			case CF_Float9995E:
//			case CF_YUV_420:
//			case CF_YUV_422:
//			case CF_YUV_444:
//			case CF_YUV_4444:
//				// toDo
//				return CMP_ERR_UNSUPPORTED_SOURCE_FORMAT;
//				break;
//			case CF_Compressed:
//				bytesPerChannel = 1;
//				break;
//			default:
//				bytesPerChannel = 4;
//				break;
//		}
//
//		pMipSet->dwDataSize = (nWidth * nHeight) * bytesPerChannel;
//
//		//----------------------------------------------------------------
//		// Allocate memory for the mip level 0
//		//----------------------------------------------------------------
//		if (!CMips.AllocateCompressedMipLevelData(pOutMipLevel, pMipSet->m_nWidth, pMipSet->m_nHeight, pMipSet->dwDataSize))
//		{
//			std::printf("Memory Error(1): allocating MIPSet compression level data buffer\n");
//			return CMP_ERR_MEM_ALLOC_FOR_MIPSET;
//		}
//
//		pMipSet->pData = pOutMipLevel->m_pbData;
//
//		return CMP_OK;
//	}
//
//	CMP_INT CMP_MaxFacesOrSlices(const CMP_MipSet* pMipSet, CMP_INT nMipLevel)
//	{
//		if (!pMipSet)
//			return 0;
//
//		if (pMipSet->m_nDepth < 1)
//			return 0;
//
//		if (pMipSet->m_TextureType == TT_2D || pMipSet->m_TextureType == TT_CubeMap)
//			return pMipSet->m_nDepth;
//
//		int nMaxSlices = pMipSet->m_nDepth;
//		for (int i = 0; i < pMipSet->m_nMipLevels; i++)
//		{
//			if (i == nMipLevel)
//				return nMaxSlices;
//
//			nMaxSlices = nMaxSlices > 1 ? nMaxSlices >> 1 : 1;  //div by 2, min of 1
//		}
//		return 0;  //nMipLevel was too high
//	}
//
//	CMP_MipLevel* CMP_CMIPS::GetMipLevel(const CMP_MipSet* pMipSet, int nMipLevel, int nFaceOrSlice)
//	{
//		if (!pMipSet)
//		{
//			assert(pMipSet);
//			return NULL;
//		}
//
//		if (!pMipSet->m_pMipLevelTable)
//		{
//			return NULL;
//		}
//
//		if (nMipLevel > MAX_MIPLEVEL_SUPPORTED)
//		{
//			return NULL;
//		}
//
//		if (nMipLevel > pMipSet->m_nMaxMipLevels)
//		{
//			assert(nMipLevel <= pMipSet->m_nMaxMipLevels);
//			return NULL;
//		}
//		if (nFaceOrSlice < 0)
//		{
//			return NULL;  //not an error, indicates requested face doesn't exist
//		}
//		int nDepth = pMipSet->m_nDepth, index = 0, whichMipLevel = 0;
//
//		switch (pMipSet->m_TextureType)
//		{
//			case TT_1D:
//			case TT_2D:
//				if (nFaceOrSlice != 0)
//				{
//					return NULL;
//				}
//				return (pMipSet->m_pMipLevelTable)[nMipLevel];
//			case TT_CubeMap:
//				if (nFaceOrSlice > 6)
//				{  //cubemap have at most 6 faces
//					assert(nFaceOrSlice > 6);
//					return NULL;
//				}
//				return (pMipSet->m_pMipLevelTable)[nMipLevel * nDepth + nFaceOrSlice];
//			case TT_VolumeTexture:
//				while (whichMipLevel <= nMipLevel)
//				{
//					if (whichMipLevel == nMipLevel)
//					{
//						return (pMipSet->m_pMipLevelTable)[index + nFaceOrSlice];
//					}
//					else
//					{
//						index += nDepth;
//						whichMipLevel++;
//						nDepth = nDepth > 1 ? nDepth >> 1 : 1;
//					}
//				}
//				return NULL;
//			default:
//				assert(0);
//				return NULL;
//		}
//	}
//
//	// Mip Levels are coded as follows
//	// 1 is original size 2 = 1/2 size 3 = 1/4 size etc...
//	// this value is based on arrays been MipMap[maxMipLevels]
//	int CMP_CMIPS::GetMaxMipLevels(int nWidth, int nHeight, int nDepth)
//	{
//		int maxMipLevels = 0;
//
//		assert(nWidth > 0 && nHeight > 0 && nDepth > 0);
//
//		while (nWidth >= 1 || nHeight >= 1 || nDepth > 1)
//		{
//			maxMipLevels++;
//
//			if (nWidth == 1 && nHeight == 1)
//				break;
//
//			//div by 2
//			nWidth = nWidth > 1 ? nWidth >> 1 : 1;
//			nHeight = nHeight > 1 ? nHeight >> 1 : 1;
//			nDepth = nDepth > 1 ? nDepth >> 1 : 1;
//		}
//
//		return maxMipLevels;
//	}
//
//	bool CMP_CMIPS::AllocateMipLevelTable(CMP_MipLevelTable** ppMipLevelTable, int nMaxMipLevels, CMP_TextureType textureType, int nDepth, int& nLevelsToAllocate)
//	{
//		//TODO test
//		assert(nDepth > 0);
//		nLevelsToAllocate = 0;
//		//determine # miplevels to allocate based on texture type
//		switch (textureType)
//		{
//			case TT_1D:
//			case TT_2D:
//				nLevelsToAllocate = nMaxMipLevels;
//				if (nDepth != 1)
//				{
//					return false;
//				}
//				break;
//			case TT_CubeMap:
//				if (nDepth > 6)
//				{
//					return false;
//				}
//				nLevelsToAllocate = nMaxMipLevels * nDepth;
//				break;
//			case TT_VolumeTexture:
//				for (int i = 0; i < nMaxMipLevels; i++)
//				{
//					nLevelsToAllocate += nDepth;
//					if (nDepth > 1)
//					{
//						nDepth >>= 1;
//					}
//				}
//				break;
//			default:
//				return false;
//		}
//		//allocate the mipLevelTable (buncha pointers to miplevels)
//		*ppMipLevelTable = reinterpret_cast<CMP_MipLevelTable*>(calloc(nLevelsToAllocate, sizeof(CMP_MipLevel*)));
//		assert(*ppMipLevelTable);
//		return (*ppMipLevelTable != NULL);
//	}
//
//	bool CMP_CMIPS::AllocateAllMipLevels(CMP_MipLevelTable* pMipLevelTable, CMP_TextureType /*textureType*/, int nLevelsToAllocate)
//	{
//		//TODO test
//		//allocate each MipLevel that the table points to
//		for (int i = 0; i < nLevelsToAllocate; i++)
//		{
//			pMipLevelTable[i] = reinterpret_cast<CMP_MipLevel*>(calloc(sizeof(CMP_MipLevel), 1));
//			//make sure it was allocated ok
//			assert(pMipLevelTable[i]);
//			if (!pMipLevelTable[i])
//			{
//				//free previous mipLevels
//				for (i -= 1; i >= 0; i--)
//				{
//					if (pMipLevelTable[i])
//					{
//						free(pMipLevelTable[i]);
//						pMipLevelTable[i] = NULL;
//					}
//				}
//				return false;
//			}
//		}
//		return true;
//	}
//
//	bool CMP_CMIPS::AllocateMipSet(CMP_MipSet* pMipSet,
//								   CMP_ChannelFormat channelFormat,
//								   TextureDataType   textureDataType,
//								   CMP_TextureType   textureType,
//								   int               nWidth,
//								   int               nHeight,
//								   int               nDepth)
//	{
//		//TODO test
//		assert(pMipSet);
//		if (!(nWidth > 0 && nHeight > 0 && nDepth > 0))
//			return false;
//
//		if (pMipSet->m_pMipLevelTable)
//		{
//			assert(!pMipSet->m_pMipLevelTable);
//			return false;
//		}
//		//depth only matters for this when its volume texture
//		pMipSet->m_nMaxMipLevels = GetMaxMipLevels(nWidth, nHeight, textureType == TT_VolumeTexture ? nDepth : 1);
//
//		if (pMipSet->m_nMipLevels > pMipSet->m_nMaxMipLevels || pMipSet->m_nMipLevels < 0)
//			pMipSet->m_nMipLevels = 0;
//
//		pMipSet->m_ChannelFormat = channelFormat;
//		pMipSet->m_TextureDataType = textureDataType;
//		pMipSet->m_TextureType = textureType;
//		//Probably shouldn't wipe this out either pMipSet->m_Flags = MS_Default;
//		//On second thought, DONT wipe this out pMipSet->m_CubeFaceMask = 0;
//		pMipSet->m_nWidth = nWidth;
//		pMipSet->m_nHeight = nHeight;
//		pMipSet->m_nDepth = nDepth;
//		int numLevelsToAllocate;
//		if (!AllocateMipLevelTable(&pMipSet->m_pMipLevelTable, pMipSet->m_nMaxMipLevels, textureType, nDepth, numLevelsToAllocate))
//		{
//			//mipleveltable allocation failed
//			return false;
//		}
//		if (!AllocateAllMipLevels(pMipSet->m_pMipLevelTable, textureType, numLevelsToAllocate))
//		{
//			//allocation of mip levels failed
//			if (pMipSet->m_pMipLevelTable)
//			{
//				free(pMipSet->m_pMipLevelTable);
//				pMipSet->m_pMipLevelTable = NULL;
//			}
//			return false;
//		}
//		return true;
//	}
//
//	bool CMP_CMIPS::AllocateMipLevelData(CMP_MipLevel* pMipLevel, int nWidth, int nHeight, CMP_ChannelFormat channelFormat, TextureDataType textureDataType)
//
//	{
//		//TODO test
//		assert(pMipLevel);
//		assert(nWidth > 0 && nHeight > 0);
//
//		CMP_DWORD dwBitsPerPixel;
//		switch (channelFormat)
//		{
//			case CF_8bit:
//			case CF_2101010:
//			case CF_1010102:
//			case CF_Float9995E:
//				dwBitsPerPixel = 8;
//				break;
//
//			case CF_16bit:
//			case CF_Float16:
//				dwBitsPerPixel = 16;
//				break;
//
//			case CF_32bit:
//			case CF_Float32:
//				dwBitsPerPixel = 32;
//				break;
//
//			default:
//				assert(0);
//				return false;
//		}
//
//		switch (textureDataType)
//		{
//			case TDT_XRGB:
//			case TDT_ARGB:
//			case TDT_NORMAL_MAP:
//				dwBitsPerPixel *= 4;
//				break;
//			case TDT_RGB:
//				dwBitsPerPixel *= 3;
//				break;
//			case TDT_RG:
//			case TDT_16:
//				dwBitsPerPixel *= 2;
//				break;
//			case TDT_R:
//			case TDT_8:
//				break;
//			default:
//				assert(0);
//				return false;
//		}
//
//		CMP_DWORD dwPitch = CMP_PAD_BYTE(nWidth, dwBitsPerPixel);
//		pMipLevel->m_nWidth = nWidth;
//		pMipLevel->m_nHeight = nHeight;
//		pMipLevel->m_dwLinearSize = dwPitch * nHeight;
//
//		pMipLevel->m_pbData = reinterpret_cast<CMP_BYTE*>(malloc(pMipLevel->m_dwLinearSize));
//
//		return (pMipLevel->m_pbData != NULL);
//	}
//
//	bool CMP_CMIPS::AllocateCompressedMipLevelData(CMP_MipLevel* pMipLevel, int nWidth, int nHeight, CMP_DWORD dwSize)
//	{
//		//TODO test
//		assert(pMipLevel);
//		assert(nWidth > 0 && nHeight > 0);
//
//		pMipLevel->m_dwLinearSize = dwSize;
//		pMipLevel->m_nWidth = nWidth;
//		pMipLevel->m_nHeight = nHeight;
//
//		pMipLevel->m_pbData = reinterpret_cast<CMP_BYTE*>(malloc(pMipLevel->m_dwLinearSize));
//
//		return (pMipLevel->m_pbData != NULL);
//	}
//
//	void CMP_CMIPS::FreeMipSet(CMP_MipSet* pMipSet)
//	{
//		//TODO test
//		int nTotalOldMipLevels = 0;
//		assert(pMipSet);
//		if (pMipSet)
//		{
//			if (pMipSet->m_pMipLevelTable)
//			{
//				//determine number of miplevels in the old mipleveltable
//				switch (pMipSet->m_TextureType)
//				{
//					case TT_1D:
//					case TT_2D:
//						nTotalOldMipLevels = pMipSet->m_nMaxMipLevels;
//						break;
//					case TT_CubeMap:
//						nTotalOldMipLevels = pMipSet->m_nMaxMipLevels * pMipSet->m_nDepth;
//						break;
//					case TT_VolumeTexture:
//						for (int depth = pMipSet->m_nDepth, mipLevels = 0; mipLevels < pMipSet->m_nMaxMipLevels; mipLevels++)
//						{
//							nTotalOldMipLevels += depth;
//							if (depth > 1)
//							{
//								depth >>= 1;
//							}
//						}
//						break;
//					default:
//						assert(0);
//				}
//				//free all miplevels and their data except the one use in gui view
//				for (int i = 0; i < nTotalOldMipLevels - 2; i++)
//				{
//					if (pMipSet->m_pMipLevelTable[i]->m_pbData)
//					{
//#ifdef USE_BASIS
//						if (pMipSet->m_format == CMP_FORMAT_BASIS)
//						{
//							CMP_VEC8& basis_data = *(pMipSet->m_pMipLevelTable[i]->m_pvec8Data);
//							if (basis_data.size() > 0)
//								delete& basis_data;
//						}
//						else
//#endif
//							free(pMipSet->m_pMipLevelTable[i]->m_pbData);
//
//						pMipSet->m_pMipLevelTable[i]->m_pbData = NULL;
//					}
//
//					if (pMipSet->m_pMipLevelTable[i])
//					{
//						free(pMipSet->m_pMipLevelTable[i]);
//						pMipSet->m_pMipLevelTable[i] = NULL;
//					}
//				}
//
//				free(pMipSet->m_pMipLevelTable);
//				pMipSet->m_pMipLevelTable = NULL;
//				pMipSet->m_nMaxMipLevels = 0;
//				pMipSet->m_nMipLevels = 0;
//			}
//		}
//	}
//
//	void CMP_CMIPS::FreeMipLevelData(CMP_MipLevel* pMipLevel)
//	{
//		if (pMipLevel->m_pbData)
//		{
//			free(pMipLevel->m_pbData);
//			pMipLevel->m_pbData = NULL;
//		}
//	}
//
//	bool CMP_CMIPS::AllocateCompressedDestBuffer(CMP_MipSet* SourceTexture, CMP_FORMAT format, CMP_MipSet* DestTexture)
//	{
//		CMP_MipLevel* pInMipLevel = GetMipLevel(SourceTexture, 0);
//		SourceTexture->dwWidth = pInMipLevel->m_nWidth;
//		SourceTexture->dwHeight = pInMipLevel->m_nHeight;
//		SourceTexture->dwDataSize = pInMipLevel->m_dwLinearSize;
//		SourceTexture->pData = pInMipLevel->m_pbData;
//		SourceTexture->m_swizzle = false;
//
//		memset(DestTexture, 0, sizeof(CMP_MipSet));
//		DestTexture->m_Flags = MS_FLAG_Default;
//
//		//----------------------------------------------------------------
//		// Allocate the compression buffer and miplevel tables
//		//----------------------------------------------------------------
//		DestTexture->m_nHeight = SourceTexture->m_nHeight;
//		DestTexture->m_nWidth = SourceTexture->m_nWidth;
//		DestTexture->dwWidth = pInMipLevel->m_nWidth;
//		DestTexture->dwHeight = pInMipLevel->m_nHeight;
//		DestTexture->m_ChannelFormat = CF_Compressed;
//		DestTexture->m_format = format;
//		DestTexture->m_dwFourCC = CMP_MIPS_FOURCC_DX10;
//		DestTexture->m_nBlockHeight = 4;
//		DestTexture->m_nBlockWidth = 4;
//		DestTexture->m_nMaxMipLevels = SourceTexture->m_nMaxMipLevels;
//		DestTexture->m_nMipLevels = 1;
//		DestTexture->m_nDepth = SourceTexture->m_nDepth;
//		if (DestTexture->m_nDepth < 1)
//			DestTexture->m_nDepth = 1;  // depthsupport on compressed data ?
//
//		if (!AllocateMipSet(DestTexture, CF_8bit, TDT_ARGB, TT_2D, SourceTexture->m_nWidth, SourceTexture->m_nHeight, 1))
//		{
//			// std::printf("Memory Error(1): allocating MIPSet Compression buffer\n");
//			return false;
//		}
//
//		//----------------------------------------------------------------
//		// Access the data table for mip level 0 (full texture width and height)
//		//----------------------------------------------------------------
//		CMP_MipLevel* pOutMipLevel = GetMipLevel(DestTexture, 0);
//
//		//----------------------------------------------------------------
//		// Calculate the target compressed block buffer size (4 bytes x 4 bytes)
//		//----------------------------------------------------------------
//		unsigned int Width = ((SourceTexture->m_nWidth + 3) / 4) * 4;
//		unsigned int Height = ((SourceTexture->m_nHeight + 3) / 4) * 4;
//		DestTexture->dwDataSize = Width * Height;
//
//		//----------------------------------------------------------------
//		// Allocate memory for the mip level 0
//		//----------------------------------------------------------------
//		if (!AllocateCompressedMipLevelData(pOutMipLevel, DestTexture->dwWidth, DestTexture->dwHeight, DestTexture->dwDataSize))
//		{
//			//std::printf("Memory Error(1): allocating MIPSet compression level data buffer\n");
//			return false;
//		}
//
//		DestTexture->pData = pOutMipLevel->m_pbData;
//		return true;
//	}
//
//	void CMP_CMIPS::SetProgress(unsigned int value)
//	{
//		if (SetProgressValue)
//		{
//			SetProgressValue(value, &m_canceled);
//		}
//	}
//
//}
