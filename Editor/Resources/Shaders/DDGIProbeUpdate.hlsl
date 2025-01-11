/*
* Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto.  Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/
#pragma stage : comp

#include <Buffers.hlslh>
#include <HostDevice.hlslh>

#include "RTXGI/ddgi/include/Descriptors.hlsl"

#include <ddgi/ProbeCommon.hlsl>
// #include "ddgi/DDGIRootConstants.hlsl"
struct PushData 
{ 
    uint VolumeIndex;
    float ProbeRadius;
    uint InstanceOffset;
    uint VolumeConstantsIndex;
    uint VolumeResourceIndicesIndex;
};

[[vk::push_constant]] PushData pushData;



[numthreads(32, 1, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
    // Get the DDGIVolume index from root/push constants
    uint volumeIndex = pushData.VolumeIndex;

    // Get the DDGIVolume structured buffers
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(pushData.VolumeConstantsIndex);
    StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(pushData.VolumeResourceIndicesIndex);

    // Load and unpack the DDGIVolume's constants
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Get the number of probes
    uint numProbes = (volume.probeCounts.x * volume.probeCounts.y * volume.probeCounts.z);

    // Early out: processed all probes, a probe doesn't exist for this thread
    if(DispatchThreadID.x >= numProbes) return;

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's grid coordinates
    float3 probeCoords = DDGIGetProbeCoords(DispatchThreadID.x, volume);

    // Get the probe's world position from the probe index
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the probe radius
    float probeRadius = pushData.ProbeRadius;

    // DebugImage[uint2(DispatchThreadID.x, 0)] = probeRadius.xxxx;

    // Get the instance offset (where one volume's probes end and another begin)
    uint instanceOffset = pushData.InstanceOffset;

    // Get the TLAS Instances structured buffer
    RWStructuredBuffer<TLASInstance> RWInstances = GetDDGIProbeVisTLASInstances();

    // Set the probe's transform
    RWInstances[(instanceOffset + DispatchThreadID.x)].transform = float3x4(
        probeRadius, 0.f, 0.f, probeWorldPosition.x,
        0.f, probeRadius, 0.f, probeWorldPosition.y,
        0.f, 0.f, probeRadius, probeWorldPosition.z);

}
