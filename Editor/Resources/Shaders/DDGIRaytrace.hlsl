/*
 * Copyright (c) 2019-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */
#pragma stage : rgen

#include <Buffers.hlslh>
#include <Common.slh>
#include <HostDevice.hlslh>
#include <PBR.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>

// #include "../include/Descriptors.hlsl"
// #include "../include/Lighting.hlsl"
// #include "../include/RayTracing.hlsl"

// #include "../../../../rtxgi-sdk/shaders/ddgi/include/DDGIRootConstants.hlsl"
// #include "../../../../rtxgi-sdk/shaders/ddgi/Irradiance.hlsl"
#include <RTXGI/Common.hlsl>
#include <RTXGI/ddgi/include/Descriptors.hlsl>
#include <RTXGI/ddgi/include/Irradiance.hlsl>
#include <RTXGI/ddgi/include/RayTracing.hlsl>
#include <ddgi/ProbeCommon.hlsl>
#include <ddgi/ProbeIndexing.hlsl>
#include <rtxgi/ddgi/DDGIVolumeDescGPU.h>

// #include <RTXGI/ddgi/Lighting.hlsl>
#include "ddgi/DDGIRootConstants.hlsl"

// ---[ Ray Generation Shader ]---

[shader("raygeneration")]
void main()
{
    // Get the DDGIVolume's index (from root/push constants)
    uint volumeIndex = GetDDGIVolumeIndex();

    // Get the DDGIVolume structured buffers
    // StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(0);
    StructuredBuffer<DDGIVolumeDescGPUPacked> DDGIVolumes = GetDDGIVolumeConstants(GetDDGIVolumeConstantsIndex());
    // StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless = GetDDGIVolumeResourceIndices(0);
    StructuredBuffer<DDGIVolumeResourceIndices> DDGIVolumeBindless =
        GetDDGIVolumeResourceIndices(GetDDGIVolumeResourceIndicesIndex());

    // Get the DDGIVolume's bindless resource indices
    DDGIVolumeResourceIndices resourceIndices = DDGIVolumeBindless[volumeIndex];

    // Get the DDGIVolume's constants from the structured buffer
    DDGIVolumeDescGPU volume = UnpackDDGIVolumeDescGPU(DDGIVolumes[volumeIndex]);

    // Compute the probe index for this thread
    int rayIndex = DispatchRaysIndex().x;        // index of the ray to trace for this probe
    int probePlaneIndex = DispatchRaysIndex().y; // index of this probe within the plane of probes
    int planeIndex = DispatchRaysIndex().z;      // index of the plane this probe is part of
    int probesPerPlane = DDGIGetProbesPerPlane(volume.probeCounts);

    int probeIndex = (planeIndex * probesPerPlane) + probePlaneIndex;

    // Get the probe's grid coordinates
    int3 probeCoords = DDGIGetProbeCoords(probeIndex, volume);

    // Adjust the probe index for the scroll offsets
    probeIndex = DDGIGetScrollingProbeIndex(probeCoords, volume);

    // Get the probe data texture array
    Texture2DArray<float4> ProbeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);

    // Get the probe's state
    float probeState = DDGILoadProbeState(probeIndex, ProbeData, volume);

    // Early out: do not shoot rays when the probe is inactive *unless* it is one of the "fixed" rays used by probe
    // classification
    if (probeState == RTXGI_DDGI_PROBE_STATE_INACTIVE && rayIndex >= RTXGI_DDGI_NUM_FIXED_RAYS)
        return;

    // Get the probe's world position
    // Note: world positions are computed from probe coordinates *not* adjusted for infinite scrolling
    float3 probeWorldPosition = DDGIGetProbeWorldPosition(probeCoords, volume, ProbeData);

    // Get a random normalized ray direction to use for a probe ray
    float3 probeRayDirection = DDGIGetProbeRayDirection(rayIndex, volume);

    // Get the coordinates for the probe ray in the RayData texture array
    // Note: probe index is the scroll adjusted index (if scrolling is enabled)
    uint3 outputCoords = DDGIGetRayDataTexelCoords(rayIndex, probeIndex, volume);

    // Setup the probe ray
    RayDesc ray;
    ray.Origin = probeWorldPosition;
    ray.Direction = probeRayDirection;
    ray.TMin = 0.f;
    ray.TMax = volume.probeMaxRayDistance;

    // Setup the ray payload
    DDGIPackedPayload packedPayload = (DDGIPackedPayload)0;

    // If classification is enabled, pass the probe's state to hit shaders through the payload
    if (volume.probeClassificationEnabled)
        packedPayload.packed0.x = probeState;

    // Get the acceleration structure
    RaytracingAccelerationStructure SceneTLAS = GetAccelerationStructure(SCENE_TLAS_INDEX);

    // Trace the Probe Ray
    TraceRay(SceneTLAS, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, packedPayload);

    // Get the ray data texture array
    RWTexture2DArray<float4> RayData = GetRWTex2DArray(resourceIndices.rayDataUAVIndex);

    // Unpack the payload
    DDGIPayload payload = UnpackPayload(packedPayload);

    // The ray missed. Store the miss radiance, set the hit distance to a large value, and exit early.
    if (packedPayload.hitT < 0.f)
    {
        // Store the ray miss
        float3 skyColor = u_EnvIrradianceTex.SampleLevel(GetBilinearWrapSampler(), ray.Direction, 0.0).rgb *
                          u_SceneData.EnvironmentMapIntensity;
        DDGIStoreProbeRayMiss(RayData, outputCoords, volume, 0.0f);
        return;
    }

    // The ray hit a surface backface
    if (payload.hitKind == HIT_KIND_TRIANGLE_BACK_FACE)
    {
        // Store the ray backface hit
        DDGIStoreProbeRayBackfaceHit(RayData, outputCoords, volume, payload.hitT);
        return;
    }

    // Early out: a "fixed" ray hit a front facing surface. Fixed rays are not blended since their direction
    // is not random and they would bias the irradiance estimate. Don't perform lighting for these rays.
    if ((volume.probeRelocationEnabled || volume.probeClassificationEnabled) && rayIndex < RTXGI_DDGI_NUM_FIXED_RAYS)
    {
        // Store the ray front face hit distance (only)
        DDGIStoreProbeRayFrontfaceHit(RayData, outputCoords, volume, payload.hitT);
        return;
    }

    float3 view = normalize(ray.Origin - payload.worldPosition);
    float NdotV = max(dot(payload.normal, view), 0.0);

    // Fresnel reflectance, metals use albedo
    float3 F0 = lerp(0.04f, payload.albedo, payload.metallic);

    DDGIPayload rayPayload = UnpackPayload(packedPayload);
    PbrMaterial material = defaultPbrMaterial(rayPayload.albedo, rayPayload.metallic, rayPayload.roughness,
                                              rayPayload.shadingNormal, rayPayload.normal);

    float3 diffuse = DirectLighting(asuint(probeRayDirection.x), F0, material, view, 1, SceneTLAS).rgb;
    diffuse += payload.albedo * payload.opacity; // Emission, not opacity.

    // Indirect Lighting (recursive)
    float3 irradiance = 0.f;
    float3 surfaceBias = DDGIGetSurfaceBias(payload.normal, ray.Direction, volume);

    // Get the volume resources needed for the irradiance query
    DDGIVolumeResources resources;
    resources.probeIrradiance = GetTex2DArray(resourceIndices.probeIrradianceSRVIndex);
    resources.probeDistance = GetTex2DArray(resourceIndices.probeDistanceSRVIndex);
    resources.probeData = GetTex2DArray(resourceIndices.probeDataSRVIndex);
    resources.bilinearSampler = GetBilinearWrapSampler();

    // Compute volume blending weight
    float volumeBlendWeight = DDGIGetVolumeBlendWeight(payload.worldPosition, volume);

    // Don't evaluate irradiance when the surface is outside the volume
    if (volumeBlendWeight > 0)
    {
        // Get irradiance from the DDGIVolume
        irradiance = DDGIGetVolumeIrradiance(payload.worldPosition, surfaceBias, payload.normal, volume, resources);

        // Attenuate irradiance by the blend weight
        irradiance *= volumeBlendWeight;
    }

    // Perfectly diffuse reflectors don't exist in the real world.
    // Limit the BRDF albedo to a maximum value to account for the energy loss at each bounce.
    float maxAlbedo = 0.9f;

    // Store the final ray radiance and hit distance
    float3 radiance =
        diffuse + ((min(payload.albedo, float3(maxAlbedo, maxAlbedo, maxAlbedo)) / RTXGI_PI) * irradiance);
    DDGIStoreProbeRayFrontfaceHit(RayData, outputCoords, volume, saturate(radiance), payload.hitT);
}

// #pragma stage : ahit
// #include <Buffers.hlslh>

// #include <rtxgi/ddgi/DDGIVolumeDescGPU.h>
// #include <ddgi/ProbeCommon.hlsl>
// #include <ddgi/ProbeIndexing.hlsl>
// #include <RTXGI/Common.hlsl>
// #include <RTXGI/ddgi/include/Descriptors.hlsl>
// #include <RTXGI/ddgi/include/Irradiance.hlsl>
// #include <RTXGI/ddgi/include/RayTracing.hlsl>
// #include "ddgi/DDGIRootConstants.hlsl"

// [shader("anyhit")]
// void main(inout DDGIPackedPayload payload, BuiltInTriangleIntersectionAttributes attrib)
// {
//     // Load the intersected mesh geometry's data
//     GeometryData geometry;
//     GetGeometryData(InstanceID(), GeometryIndex(), geometry);

//     // Load the surface material
//     Material material = GetMaterial(geometry);

//     float alpha = material.opacity;
//     if (material.alphaMode == 2)
//     {
//         // Load the vertices
//         Vertex vertices[3];
//         LoadVerticesPosUV0(InstanceID(), PrimitiveIndex(), geometry, vertices);

//         // Interpolate the triangle's texture coordinates
//         float3 barycentrics = float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x,
//         attrib.barycentrics.y); Vertex v = InterpolateVertexUV0(vertices, barycentrics);

//         // Sample the texture
//         if (material.albedoTexIdx > -1)
//         {
//             // Get the number of mip levels
//             uint width, height, numLevels;
//             GetTex2D(material.albedoTexIdx).GetDimensions(0, width, height, numLevels);

//             // Sample the texture
//             alpha *= GetTex2D(material.albedoTexIdx).SampleLevel(GetBilinearWrapSampler(), v.uv0, numLevels *
//             0.6667f).a;
//         }
//     }

//     if (alpha < material.alphaCutoff) IgnoreHit();
// }

#pragma stage : chit
#include <Buffers.hlslh>

#include <HostDevice.hlslh>
#include <Raytracing/Vertex.hlslh>

#include "ddgi/DDGIRootConstants.hlsl"
#include <RTXGI/Common.hlsl>
#include <RTXGI/ddgi/include/Descriptors.hlsl>
#include <RTXGI/ddgi/include/Irradiance.hlsl>
#include <RTXGI/ddgi/include/RayTracing.hlsl>
#include <ddgi/ProbeCommon.hlsl>
#include <ddgi/ProbeIndexing.hlsl>
#include <rtxgi/ddgi/DDGIVolumeDescGPU.h>

[shader("closesthit")]
void main(inout DDGIPackedPayload packedPayload, BuiltInTriangleIntersectionAttributes attrib)
{
    DDGIPayload payload = (DDGIPayload)0;
    payload.hitT = RayTCurrent();
    payload.hitKind = HitKind();

    uint2 launchIndex = DispatchRaysIndex().xy;

    // Object data
    ObjDesc objResource = objDescs[InstanceID()];
    float3 barycentrics =
        float3((1.f - attrib.barycentrics.x - attrib.barycentrics.y), attrib.barycentrics.x, attrib.barycentrics.y);

    Vertex vertex = LoadInterpolatedVertex(objResource, PrimitiveIndex(), barycentrics);

    // World position
    payload.worldPosition = vertex.Position;
    payload.worldPosition = mul(ObjectToWorld3x4(), float4(payload.worldPosition, 1.f)).xyz; // instance transform

    // Geometric normal
    payload.normal = vertex.Normal;
    payload.normal = normalize(mul(ObjectToWorld3x4(), float4(payload.normal, 0.f)).xyz);
    payload.shadingNormal = payload.normal;

    // Load the surface material
    Material material = materials[objResource.MaterialIndex];

    payload.albedo = material.AlbedoColor.rgb;
    // payload.opacity = material.opacity;

    // Albedo and Opacity
    if (material.AlbedoTexIndex > 4)
    {
        // Get the number of mip levels
        uint width, height, numLevels;
        GetTex2D(material.AlbedoTexIndex).GetDimensions(0, width, height, numLevels);

        // Sample the albedo texture
        float4 bco =
            GetTex2D(material.AlbedoTexIndex).SampleLevel(GetBilinearWrapSampler(), vertex.TexCoord, numLevels / 2.f);
        payload.albedo *= bco.rgb;
        payload.opacity *= bco.a;
    }

    payload.albedo += payload.albedo * material.Emission;
    payload.opacity = material.Emission;

    // Shading normal
    if (material.NormalTexIndex > 4)
    {
        // Get the number of mip levels
        uint width, height, numLevels;
        GetTex2D(material.NormalTexIndex).GetDimensions(0, width, height, numLevels);

        float3 tangent = normalize(mul(ObjectToWorld3x4(), float4(vertex.Tangent.xyz, 0.f)).xyz);
        float3 biNormal = normalize(mul(ObjectToWorld3x4(), float4(vertex.Binormal.xyz, 0.f)).xyz);
        float3x3 TBN = { tangent, biNormal, payload.normal };
        payload.shadingNormal = GetTex2D(material.NormalTexIndex)
                                    .SampleLevel(GetBilinearWrapSampler(), vertex.TexCoord, numLevels / 2.f)
                                    .xyz;
        payload.shadingNormal = (payload.shadingNormal * 2.f) - 1.f; // Transform to [-1, 1]
        payload.shadingNormal = mul(payload.shadingNormal, TBN);     // Transform tangent-space normal to world-space
    }

    // Pack the payload
    packedPayload = PackPayload(payload);
}

#pragma stage : miss

// #include "include/Descriptors.hlsl"
// #include "DDGI/RayTracing.hlsl"

#include "Types.hlslh"

// ---[ Miss Shader ]---
[shader("miss")]
void main(inout DDGIPackedPayload packedPayload)
{
    packedPayload.hitT = -1.f;
}
