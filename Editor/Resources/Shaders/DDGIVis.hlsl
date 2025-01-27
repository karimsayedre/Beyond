#pragma stage : rgen

/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#include <Buffers.hlslh>
#include <HostDevice.hlslh>

// #include "../../include/Common.hlsl"
#include "RTXGI/ddgi/include/Descriptors.hlsl"
#include "RTXGI/ddgi/include/RayTracing.hlsl"

#include "ddgi/DDGIRootConstants.hlsl"
#include <ddgi/ProbeCommon.hlsl>

VK_BINDING(2, 1) RWTexture2D<float4> GBufferA;
VK_BINDING(3, 1) Texture2D DepthBuffer;

static const float TWO_PI = 6.2831853071795864f;

static const float COMPOSITE_FLAG_IGNORE_PIXEL = 0.2f;
static const float COMPOSITE_FLAG_POSTPROCESS_PIXEL = 0.5f;
static const float COMPOSITE_FLAG_LIGHT_PIXEL = 0.8f;

#define RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE 0
#define RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE 1

float3 LessThan(float3 f, float value)
{
    return float3((f.x < value) ? 1.f : 0.f, (f.y < value) ? 1.f : 0.f, (f.z < value) ? 1.f : 0.f);
}

float3 LinearToSRGB(float3 rgb)
{
    rgb = clamp(rgb, 0.f, 1.f);
    return lerp(pow(rgb * 1.055f, 1.f / 2.4f) - 0.055f, rgb * 12.92f, LessThan(rgb, 0.0031308f));
}

float LinearizeDepth(const float screenDepth)
{
    float depthLinearizeMul = u_Camera.DepthUnpackConsts.x;
    float depthLinearizeAdd = u_Camera.DepthUnpackConsts.y;
    // Optimised version of "-cameraClipNear / (cameraClipFar - projDepth * (cameraClipFar - cameraClipNear)) *
    // cameraClipFar"
    return depthLinearizeMul / (depthLinearizeAdd - screenDepth);
}

// ---[ Helpers ]---

float3 GetProbeData(int probeIndex, int3 probeCoords, float3 worldPosition, DDGIVolumeResourceIndices resourceIndices,
                    DDGIVolumeDescGPU volume, out float3 sampleDirection)
{
    float3 color = float3(0.f, 0.f, 0.f);

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's world-space position
    float3 probePosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get the octahedral coordinates for the direction
    sampleDirection = normalize(worldPosition - probePosition);
    float2 octantCoords = DDGIGetOctahedralCoordinates(sampleDirection);

    // Get the probe data type to visualize
    // uint type = GetGlobalConst(ddgivis, probeType);
    const uint type = RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE; // TODO: Should get it from the push constant?
    if (type == RTXGI_DDGI_VISUALIZE_PROBE_IRRADIANCE)
    {
        // Get the volume's irradiance texture array
        Texture2DArray<float4> ProbeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);

        // Get the texture array uv coordinates for the octant of the probe
        float3 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumIrradianceInteriorTexels, volume);

        // Sample the irradiance texture
        color = ProbeIrradiance.SampleLevel(GetBilinearWrapSampler(), uv, 0).rgb;

        // Decode the tone curve
        float3 exponent = volume.probeIrradianceEncodingGamma * 0.5f;
        color = pow(color, exponent);

        // Go back to linear irradiance
        color *= color;

        // Multiply by the area of the integration domain (2PI) to complete the irradiance estimate. Divide by PI to
        // normalize for the display.
        color *= 2.f;

        // Adjust for energy loss due to reduced precision in the R10G10B10A2 irradiance texture format
        if (volume.probeIrradianceFormat == RTXGI_DDGI_VOLUME_TEXTURE_FORMAT_U32)
        {
            color *= 1.0989f;
        }
    }
    else if (type == RTXGI_DDGI_VISUALIZE_PROBE_DISTANCE)
    {
        // Get the volume's distance texture array
        Texture2DArray<float4> ProbeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);

        // Get the texture array uv coordinates for the octant of the probe
        float3 uv = DDGIGetProbeUV(probeIndex, octantCoords, volume.probeNumDistanceInteriorTexels, volume);

        // Sample the distance texture and reconstruct the depth
        float distance = 2.f * ProbeDistance.SampleLevel(GetBilinearWrapSampler(), uv, 0).r;

        // Normalize the distance for visualization
        float value = saturate(distance / 5.0 /*GetGlobalConst(ddgivis, distanceDivisor)*/);
        color = float3(value, value, value);
    }

    return color;
}

void WriteResult(uint2 LaunchIndex, float3 color, float hitT, RWTexture2D<float4> GBufferAOutput
                 /*, RWTexture2D<float4> GBufferBOutput*/)
{
    // Convert from linear to sRGB
    color = LinearToSRGB(color);

    // Overwrite GBufferA's albedo and mark the pixel to not be lit or post processed
    GBufferAOutput[LaunchIndex] = float4(color, COMPOSITE_FLAG_IGNORE_PIXEL);

    // Overwrite GBufferB's hit distance with the distance to the probe
    // GBufferBOutput[LaunchIndex].w = hitT;
}

// ---[ Ray Generation Shaders ]---

[shader("raygeneration")]
void main()
{
    uint2 LaunchIndex = DispatchRaysIndex().xy;
    uint2 LaunchDimensions = DispatchRaysDimensions().xy;

    // RaytracingAccelerationStructure DDGIProbeVisTLAS = TLAS;

    // Setup the primary ray

    const float2 pixelCenter = float2(LaunchIndex.xy) + float2(0.5, 0.5);
    const float2 inUV = pixelCenter / float2(LaunchDimensions.xy);

    float2 d = inUV * 2.0 - 1.0;
    float4 origin = mul(u_Camera.InverseViewMatrix, float4(0, 0, 0, 1));
    float4 target = mul(u_Camera.InverseProjectionMatrix, float4(d.x, d.y, 1, 1));
    float4 direction = mul(u_Camera.InverseViewMatrix, float4(normalize(target.xyz), 0));

    RayDesc ray;
    ray.TMin = 0.f;
    ray.TMax = 1e27f;
    ray.Origin = origin.xyz;
    ray.Direction = direction.xyz;

    // Trace
    ProbeVisualizationPayload payload = (ProbeVisualizationPayload)0;

    TraceRay(TLAS, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0x2, 0, 0, 0, ray, payload);

    if (payload.hitT > 0.f)
    {

        // If the GBuffer doesn't contain geometry or a visualization
        // probe is hit by a primary ray - and the probe is the
        // closest surface - overwrite GBufferA with probe information.
        float depth = DepthBuffer.SampleLevel(GetBilinearWrapSampler(), inUV * u_ScreenData.HZBUVFactor, 0.0).x;

        float linearDepth = LinearizeDepth(depth);

        Bey_DebugImage[LaunchIndex.xy] = float4(length(linearDepth.xxx - payload.hitT.xxx).xxx, 1.0);
        if (linearDepth < 0.0 || payload.hitT <= linearDepth)
        {
            // Get the DDGIVolume index
            uint volumeIndex = payload.volumeIndex;

            // Get the DDGIVolume structured buffers
            StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes =
                GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
            StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless =
                GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

            // Get the DDGIVolume's bindless resource indices
            DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

            // Load the DDGIVolume constants
            DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

            // Adjust for all volume probe instances existing in a single TLAS
            int probeIndex = (payload.instanceOffset);

            // Get the probe's grid coordinates
            int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

            // Adjust probe index for scroll offsets
            probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

            // Get the probe's data to display
            float3 sampleDirection;

            float3 color =
                GetProbeData(probeIndex, probeCoords, payload.worldPosition, resourceIndices, volume, sampleDirection);

            // Color the probe if classification is enabled
            if (volume.probeClassificationEnabled)
            {
                const float3 INACTIVE_COLOR = float3(1.f, 0.f, 0.f); // Red
                const float3 ACTIVE_COLOR = float3(0.f, 1.f, 0.f);   // Green

                // Get the probe data texture array
                Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

                // Get the probe's location in the probe data texture
                uint3 probeStateTexCoords = DDGIGetProbeTexelCoords(probeIndex, volume);

                // Get the probe's state
                float probeState = ProbeData[probeStateTexCoords].w;

                // Probe coloring
                if (abs(dot(ray.Direction, sampleDirection)) < 0.45f)
                {
                    if (probeState == RTXGI_DDGI_PROBE_STATE_ACTIVE)
                    {
                        color = ACTIVE_COLOR;
                    }
                    else if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE)
                    {
                        color = INACTIVE_COLOR;
                    }
                }
            }
            color = any(color < 0.0f) ? GBufferA[LaunchIndex].rgb : color;

            // Write the result to the GBuffer
            WriteResult(LaunchIndex, color, payload.hitT, GBufferA);
        }
    }
}

#pragma stage : chit

struct ProbeVisualizationPayload
{
    float hitT;
    float3 worldPosition;
    int instanceIndex;
    uint volumeIndex;
    uint instanceOffset;
};

[shader("closesthit")]
void main(inout ProbeVisualizationPayload payload, BuiltInTriangleIntersectionAttributes attrib)
{
    payload.hitT = RayTCurrent();
    payload.instanceIndex = (int)InstanceIndex();
    payload.volumeIndex = (InstanceID() >> 16);
    payload.instanceOffset = (InstanceID() & 0xFFFF);
    payload.worldPosition = WorldRayOrigin() + (WorldRayDirection() * payload.hitT);
}

#pragma stage : miss

struct ProbeVisualizationPayload
{
    float hitT;
    float3 worldPosition;
    int instanceIndex;
    uint volumeIndex;
    uint instanceOffset;
};

[shader("miss")]
void main(inout ProbeVisualizationPayload payload)
{
    payload.hitT = -1.f;
}
