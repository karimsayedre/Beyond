/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef DESCRIPTORS_HLSL
#define DESCRIPTORS_HLSL

#include <Raytracing/Descriptors.hlslh>

#include "Types.h"
#include "rtxgi/ddgi/DDGIVolumeDescGPU.h"

// #include "rtxgi/Types.h"
#include <RTXGI/Platform.hlsl>

#define VK_BINDING(x, y) [[vk::binding(x, y)]]
#define VK_PUSH_CONST [[vk::push_constant]]

#define RTXGI_BINDLESS_TYPE_RESOURCE_ARRAYS 0
#define RTXGI_BINDLESS_TYPE_DESCRIPTOR_HEAP 1

#ifndef RTXGI_BINDLESS_TYPE
#error Required define RTXGI_BINDLESS_TYPE is not defined!
#endif

struct TLASInstance
{
#pragma pack_matrix(row_major)
    float3x4 transform;
#pragma pack_matrix(column_major)
    uint instanceID24_Mask8;
    uint instanceContributionToHitGroupIndex24_Flags8;
    uint2 blasAddress;
};

// Global Root / Push Constants ------------------------------------------------------------------------------------

// VK_PUSH_CONST ConstantBuffer<GlobalConstants> GlobalConst : register(b0, space0);

// #define GetGlobalConst(x, y) (GlobalConst.x##_##y)

// uint GetPTNumBounces() { return (GetGlobalConst(pt, numBounces) &  0x7FFFFFFF); }
// uint GetPTProgressive() { return (GetGlobalConst(pt, numBounces) & 0x80000000); }

// uint GetPTSamplesPerPixel() { return (GetGlobalConst(pt, samplesPerPixel) & 0x3FFFFFFF); }
// uint GetPTAntialiasing() { return (GetGlobalConst(pt, samplesPerPixel) & 0x80000000); }
// uint GetPTShaderExecutionReordering() { return GetGlobalConst(pt, samplesPerPixel) & 0x40000000; }

// uint HasDirectionalLight() { return GetGlobalConst(lighting, hasDirectionalLight); }
// uint GetNumPointLights() { return GetGlobalConst(lighting, numPointLights); }
// uint GetNumSpotLights() { return GetGlobalConst(lighting, numSpotLights); }

//----------------------------------------------------------------------------------------------------------------
// Root Signature Descriptors and Mappings
// ---------------------------------------------------------------------------------------------------------------

// Samplers -------------------------------------------------------------------------------------------------

// Constant Buffers -----------------------------------------------------------------------------------------

// VK_BINDING(1, 2) ConstantBuffer<Camera>                      CameraCB            : register(b1, space0);

// Structured Buffers ---------------------------------------------------------------------------------------

// VK_BINDING(2, 0) StructuredBuffer<PointLight>                     Lights              : register(t2, space0);
// VK_BINDING(3, 0) StructuredBuffer<Material>                  Materials           : register(t3, space0);
// VK_BINDING(4, 0) StructuredBuffer<TLASInstance>              TLASInstances       : register(t4, space0);
VK_BINDING(5, 2) StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes : register(t5, space0);
VK_BINDING(6, 2) StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless : register(t6, space0);

VK_BINDING(9, 3) RWStructuredBuffer<TLASInstance> RWTLASInstances : register(u5, space0);

// Bindless Resources ---------------------------------------------------------------------------------------

// VK_BINDING(3, 2) RWTexture2D<float4>                         GBufferB;
VK_BINDING(1, 4) RWTexture2DArray<float4> RWTex2DArray[] : register(u6, space1);
VK_BINDING(2, 4) Texture2DArray Tex2DArray[] : register(t7, space2);

// VK_BINDING(11, 4) Texture2D                                  Tex2D[]             : register(t7, space1);

// Defines for Convenience ----------------------------------------------------------------------------------

#define PT_OUTPUT_INDEX 0
#define PT_ACCUMULATION_INDEX 1
#define GBUFFERA_INDEX 2
#define GBUFFERB_INDEX 3
#define GBUFFERC_INDEX 4
#define GBUFFERD_INDEX 5
#define RTAO_OUTPUT_INDEX 6
#define RTAO_RAW_INDEX 7
#define DDGI_OUTPUT_INDEX 8

#define SCENE_TLAS_INDEX 0
#define DDGIPROBEVIS_TLAS_INDEX 1

#define BLUE_NOISE_INDEX 0

#define SPHERE_INDEX_BUFFER_INDEX 0
#define SPHERE_VERTEX_BUFFER_INDEX 1
#define MESH_OFFSETS_INDEX 2
#define GEOMETRY_DATA_INDEX 3
#define GEOMETRY_BUFFERS_INDEX 4

// Resource Accessor Functions ------------------------------------------------------------------------------

#define GetCamera() CameraCB

// StructuredBuffer<Light> GetLights() { return Lights; }

// void GetGeometryData(uint meshIndex, uint geometryIndex, out GeometryData geometry)
// {
//     uint address = ByteAddrBuffer[MESH_OFFSETS_INDEX].Load(meshIndex * 4); // address of the Mesh in the GeometryData
//     buffer address += geometryIndex * 12; // offset to mesh primitive geometry, GeometryData stride is 12 bytes

//     geometry.materialIndex = ByteAddrBuffer[GEOMETRY_DATA_INDEX].Load(address);
//     geometry.indexByteAddress = ByteAddrBuffer[GEOMETRY_DATA_INDEX].Load(address + 4);
//     geometry.vertexByteAddress = ByteAddrBuffer[GEOMETRY_DATA_INDEX].Load(address + 8);
// }
// Material GetMaterial(GeometryData geometry) { return Materials[geometry.materialIndex]; }

StructuredBuffer<DDGIVolumeDescGPUPacked> GetDDGIVolumeConstants(uint index)
{
    return DDGIVolumes;
}
StructuredBuffer<DDGIVolumeResourceIndices> GetDDGIVolumeResourceIndices(uint index)
{
    return DDGIVolumeBindless;
}

RWStructuredBuffer<TLASInstance> GetDDGIProbeVisTLASInstances()
{
    return RWTLASInstances;
}

RaytracingAccelerationStructure GetAccelerationStructure(uint index)
{
    return TLAS;
}

// ByteAddressBuffer GetSphereIndexBuffer() { return ByteAddrBuffer[SPHERE_INDEX_BUFFER_INDEX]; }
// ByteAddressBuffer GetSphereVertexBuffer() { return ByteAddrBuffer[SPHERE_VERTEX_BUFFER_INDEX]; }

// ByteAddressBuffer GetIndexBuffer(uint meshIndex) { return ByteAddrBuffer[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2)]; }
// ByteAddressBuffer GetVertexBuffer(uint meshIndex) { return ByteAddrBuffer[GEOMETRY_BUFFERS_INDEX + (meshIndex * 2) +
// 1]; }

// Bindless Resource Array Accessors ------------------------------------------------------------------------

// RWTexture2D<float4> GetRWTex2D(uint index) { return RWTex2D[index]; }

RWTexture2DArray<float4> GetRWTex2DArray(uint index)
{
    return RWTex2DArray[index];
}
Texture2DArray<float4> GetTex2DArray(uint index)
{
    return Tex2DArray[index];
}

#endif // DESCRIPTORS_HLSL
