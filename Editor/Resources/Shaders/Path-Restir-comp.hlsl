#pragma stage : comp

// #define BEY_SER 0
#define USE_RAYQUERY 1
static uint2 launchIndex;

#include <Common.hlslh>
// #include <SER.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/GGX.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RayQuery.hlslh>
#include <Raytracing/RaytracingMath.hlslh>
#include <Raytracing/Vertex.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;

static const int NBSAMPLES = 1;
static const float Fdielectric = 0.04f;
// static const int MIN_BOUNCES = 1;
static const int MAX_BOUNCES = 15;
static const float MIN_THROUGHPUT = 0.005f;

PbrMaterial TraceRay(RaytracingAccelerationStructure TLAS, RayDesc ray, uint bounceIndex, inout uint seed)
{
    RayQuery<RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> rayQuery;
    rayQuery.TraceRayInline(TLAS, RAY_FLAG_NONE, 0xFF, ray);
    while (rayQuery.Proceed())
    {
        if (rayQuery.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            if (!AnyHitRayQuery(rayQuery, seed))
                continue;

            rayQuery.CommitNonOpaqueTriangleHit();
        }
    }

    if (rayQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        return GetMaterialParams(ray, rayQuery.CommittedObjectToWorld3x4(), rayQuery.CommittedTriangleBarycentrics(),
                                 rayQuery.CommittedRayT(), rayQuery.CommittedInstanceID(),
                                 rayQuery.CommittedPrimitiveIndex(), launchIndex);
    }
    else
    {
        PbrMaterial material;
        material.HitT = -1.0f;
        return material;
    }
}

float3 TracePath(inout RayDesc ray, inout uint seed)
{
    bool isInside = false;
    float3 throughput = 1.xxx;
    float3 color = 0.0.xxx;

    [loop]
    for (int bounceIndex = 0; bounceIndex < MAX_BOUNCES; ++bounceIndex)
    {
        seed += bounceIndex;

        PbrMaterial material = TraceRay(TLAS, ray, bounceIndex, seed);

        const float3 view = -ray.Direction;

        isInside = dot(material.Ng, view) < 0.0;

        [branch]
        if (bounceIndex == 0)
        {
            o_ViewNormalsLuminance[launchIndex] =
                float4(mul((float3x3)(u_Camera.ViewMatrix), normalize(material.N)) * 0.5 + 0.5, 1.0);
            o_MetalnessRoughness[launchIndex] = float4(material.metallic, material.roughness.r, 0.0, 1.0);
            o_AlbedoColor[launchIndex] = float4(material.baseColor, material.opacity);
        }
        o_PrimaryHitT[launchIndex] = float4(material.HitT, 0.0, 0.0, 0.0);

        float NdotV = max(dot(material.N, view), 0.0);
        float3 F0 = lerp(Fdielectric.xxx, material.baseColor.rgb, material.metallic);

        // Miss, exit loop
        [branch]
        if (material.HitT < 0.f)
        {
            color += EnvironmentSample(F0, ray.Direction, bounceIndex, material) * throughput;
            return color;
        }

        bool thin_walled = (material.thickness == 0);
        if (isInside && !thin_walled)
        {
            const float3 abs_coeff = AbsorptionCoefficient(material.attenuationDistance, material.attenuationColor);
            throughput.x *= abs_coeff.x > 0.0 ? exp(-abs_coeff.x * material.HitT) : 1.0;
            throughput.y *= abs_coeff.y > 0.0 ? exp(-abs_coeff.y * material.HitT) : 1.0;
            throughput.z *= abs_coeff.z > 0.0 ? exp(-abs_coeff.z * material.HitT) : 1.0;
        }

        color += material.emissive * throughput;
        color += DirectLighting(seed, F0, material, view, 1, TLAS).rgb * throughput;

        // Sample BSDF
        {
            BsdfSampleData sampleData;
            sampleData.k1 = view;
            sampleData.xi = RandomFloat3(seed);

            bsdfSample(sampleData, material);

            throughput *= sampleData.bsdf_over_pdf;
            ray.Direction = sampleData.k2;

            if (sampleData.event_type == BSDF_EVENT_ABSORB)
            {
                break; // Need to add the contribution ?
            }
            else
            {
                // // Continue path
                // bool isSpecular = (sampleData.event_type & BSDF_EVENT_SPECULAR) != 0;
                // bool isTransmission = (sampleData.event_type & BSDF_EVENT_TRANSMISSION) != 0;

                // if (isTransmission)
                // {
                // 	isInside = !isInside;
                // }
            }
        }

        throughput *= min(material.baseColor.rgb, 0.9.xxx);

        [branch]
        if (pushConst.EnableRussianRoulette)
        {
            float rrPcont = (max(throughput.x, max(throughput.y, throughput.z)) + 0.001F);

            if (RandomFloat(seed) > rrPcont)
                break; // Terminate the path

            throughput /= rrPcont; // Adjust throughput for Russian Roulette
        }

        ray.TMin = 1e-4 * max(1.0f, length(ray.Direction));
        float3 offsetDir = dot(ray.Direction, material.Ng) > 0 ? material.Ng : -material.Ng;
        ray.Origin = offsetRay(material.WorldPosition, offsetDir);
        // ray.Origin = material.WorldPosition + ray.Direction * ray.TMin;
    }
    return color;
}

const static uint NUM_THREADS = 8;
[numthreads(NUM_THREADS, NUM_THREADS, 1)]
[shader("compute")]
void main(uint3 dispatchThreadID: SV_DispatchThreadID)
{
    launchIndex = dispatchThreadID.xy;
    uint2 launchDimensions = (uint2)u_ScreenData.FullResolution.xy;
    float4 previousColor = io_AccumulatedColor[launchIndex];

    uint seed = tea(launchIndex.y * launchDimensions.x + launchIndex.x, pushConst.FrameIndex * NBSAMPLES);

    o_ViewNormalsLuminance[launchIndex] = 0.0.xxxx;
    Bey_DebugImage[launchIndex] = 0.0.xxxx;
    float3 color = 0.0.xxx;

    [unroll]
    for (int smpl = 0; smpl < NBSAMPLES; smpl++)
    {
        // float2 subpixel_jitter = float2(0.5f, 0.5f) + float2(rnd(seed), rnd(seed));
        float2 pixelCenter = float2(launchIndex);
        float2 inUV = pixelCenter / float2(launchDimensions);

        float2 d = inUV * 2.0 - 1.0;
        float4 target = mul(u_Camera.InverseProjectionMatrix, float4(d.x, d.y, 1, 1));
        float4 direction = mul(u_Camera.InverseViewMatrix, float4(normalize(target.xyz), 0));

        RayDesc ray;
        ray.TMin = 0.001;
        ray.TMax = 1e27f;
        ray.Origin = u_SceneData.CameraPosition;
        ray.Direction = direction.xyz;
        color += TracePath(ray, seed);

        // Removing fireflies
        float lum = dot(color, float3(0.212671f, 0.715160f, 0.072169f));
        if (lum > 10.0 /*pushConst.fireflyClampThreshold*/)
        {
            color *= 10.0 /*pushConst.fireflyClampThreshold*/ / lum;
        }
    }

    color /= (NBSAMPLES);

    float numPaths = (float)NBSAMPLES;

    [branch]
    if (pushConst.PathtracingFrameIndex > 1)
    {
        color += previousColor.rgb;
        numPaths += previousColor.a;
    }

    io_AccumulatedColor[launchIndex] = float4(color, numPaths);
    o_Image[launchIndex] = float4(color * rcp(numPaths), 1.0f);
}
