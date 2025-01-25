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
// -------- CONFIGURATION DEFINES -----------------------------------------------------------------

#define RTXGI_DDGI_NUM_VOLUMES 1
#define THGP_DIM_X 8
#define THGP_DIM_Y 4

// RTXGI_DDGI_NUM_VOLUMES must be passed in as a define at shader compilation time.
// This define specifies the number of DDGIVolumes in the scene.
// Ex: RTXGI_DDGI_NUM_VOLUMES 6
#ifndef RTXGI_DDGI_NUM_VOLUMES
#error Required define RTXGI_DDGI_NUM_VOLUMES is not defined for IndirectCS.hlsl!
#endif

// THGP_DIM_X must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_X 8
#ifndef THGP_DIM_X
#error Required define THGP_DIM_X is not defined for IndirectCS.hlsl!
#endif

// THGP_DIM_Y must be passed in as a define at shader compilation time.
// This define specifies the number of threads in the thread group in the X dimension.
// Ex: THGP_DIM_Y 4
#ifndef THGP_DIM_Y
#error Required define THGP_DIM_Y is not defined for IndirectCS.hlsl!
#endif

// -------------------------------------------------------------------------------------------

// #include "include/Common.hlsl"

#include "RTXGI/ddgi/include/Descriptors.hlsl"
#include <Buffers.hlslh>
#include <HostDevice.hlslh>

VK_BINDING(0, 3) Texture2D<float4> AlbedoTexture;
[[vk::combinedImageSampler]]
VK_BINDING(1, 3) Texture2D<float4> DepthTexture;
[[vk::combinedImageSampler]]
VK_BINDING(1, 3) SamplerState SamplerPointClamp;
VK_BINDING(2, 3) Texture2D<float4> NormalTexture;
VK_BINDING(3, 3) RWTexture2D<float4> OutputTexture;

struct GTAOConstants
{
    float2 HZBUVFactor;
};
[[vk::push_constant]]
ConstantBuffer<GTAOConstants> u_GTAOConsts;

#include "RTXGI/ddgi/include/Irradiance.hlsl"

#include <ddgi/ProbeCommon.hlsl>
// #include "ddgi/DDGIRootConstants.hlsl"

float3 LessThan(float3 f, float value)
{
    return float3((f.x < value) ? 1.f : 0.f, (f.y < value) ? 1.f : 0.f, (f.z < value) ? 1.f : 0.f);
}

float3 LinearToSRGB(float3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return lerp(pow(rgb * 1.055f, 1.f / 2.4f) - 0.055f, rgb * 12.92f, LessThan(rgb, 0.0031308f));
}

float3 SRGBToLinear(float3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return lerp(pow((rgb + 0.055f) / 1.055f, 2.4f), rgb / 12.92f, LessThan(rgb, 0.04045f));
}

// ---[ Compute Shader ]---

[numthreads(THGP_DIM_X, THGP_DIM_Y, 1)]
void main(uint3 DispatchThreadID: SV_DispatchThreadID)
{
    float3 color = float3(0.f, 0.f, 0.f);
    DispatchID = DispatchThreadID.xy;

    // DebugImage[DispatchID.xy] = float4(1.0, 0.0, 0.0, 1.0);

    // Load the albedo and primary ray hit distance
    float4 albedo = AlbedoTexture.Load(DispatchThreadID.xyz);
    // DebugImage[DispatchID.xy] = float4(0.0.xxx, 1.0);

    // Primary ray hit, need to light it
    if (albedo.a > 0.f)
    {
        // Convert albedo back to linear
        // albedo.rgb = SRGBToLinear(albedo.rgb);

        // Load the world position, hit distance, and normal
        float4 positionSS = float4((DispatchThreadID.xy + 0.5) * u_ScreenData.InvFullResolution, 0.0, 1.0);
        positionSS.z = DepthTexture.SampleLevel(Samplers[0], positionSS.xy * u_GTAOConsts.HZBUVFactor, 0.0).r;
        positionSS.xy = positionSS.xy * 2.0 - 1.0;
        float4 positionWS = mul(u_Camera.InverseViewProjectionMatrix, positionSS);
        positionWS.xyz /= positionWS.w;

        float3 normal =
            normalize(mul((float3x3)(u_Camera.InverseViewMatrix), float3(NormalTexture.Load(DispatchThreadID.xyz).xyz)))
                .xyz;

        // Compute indirect lighting
        float3 irradiance = 0.f;

        // Get the structured buffers
        StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(0);
        StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(0);

        // TODO: sort volumes by density, screen-space area, and/or other prioritization heuristics
        for (int volumeIndex = 0; volumeIndex < RTXGI_DDGI_NUM_VOLUMES; volumeIndex++)
        {
            // Get the DDGIVolume's resource indices
            DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

            // Get the volume's constants
            DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

            float3 cameraDirection = normalize(positionWS.xyz - u_SceneData.CameraPosition);
            float3 surfaceBias = DDGIGetSurfaceBias(normal, cameraDirection, volume);

            // Get the volume's resources
            DDGIVolumeResources resources;
            resources.probeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);
            resources.probeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);
            resources.probeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);
            resources.bilinearSampler = GetBilinearWrapSampler();

            // Get the blend weight for this volume's contribution to the surface
            float blendWeight = DDGIGetVolumeBlendWeight(positionWS.xyz, volume);

            if (blendWeight > 0)
            {
                // Get irradiance for the world-space position in the volume
                irradiance += DDGIGetVolumeIrradiance(positionWS.xyz, surfaceBias, normal, volume, resources);
                irradiance *= blendWeight;
            }
        }

        // Compute final color
        color = (albedo.rgb / RTXGI_PI) * irradiance;
    }

    float3 old_color = OutputTexture.Load(DispatchThreadID.xy).xyz;
    OutputTexture[DispatchThreadID.xy] = float4((old_color + color), 1.f);
}
