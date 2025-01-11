#pragma stage : rgen

#define BEY_SER 0

static uint2 launchIndex;

#include <Common.hlslh>
#include <SER.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RaytracingMath.hlslh>
#include <Raytracing/GGX.hlslh>
#include <Raytracing/Vertex.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;
// [[vk::binding(12, 1)]] RWTexture2D<float4> o_MotionVectors : register(u1);

static const int NBSAMPLES = 1;
static const float Fdielectric = 0.04f;
static const int MIN_BOUNCES = 1;
static const int MAX_BOUNCES = 10;
static const float MIN_THROUGHPUT = 0.005f;

// Function to apply Russian Roulette
bool ApplyRussianRoulette(in uint seed, int bounceCount, float continueProbability)
{
	if (bounceCount < MIN_BOUNCES)
	{
		return true; // Continue tracing
	}

	// Calculate the probability of continuing

	// Decide whether to terminate the path
	return RandomFloat(seed) < continueProbability;
}

Payload TraceRay(RaytracingAccelerationStructure TLAS, RayDesc ray, uint bounceIndex)
{
#if BEY_SER
	packedPayload = (Payload)0;
	// packedPayload.PackedDepth = bounceIndex & 0xFF;

	createHitObjectNV();

	HitObjectNV hObj;
	hitObjectRecordEmptyNV(hObj);
	hitObjectTraceRayNV(hObj, TLAS,
	                    RAY_FLAG_NONE // RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
	                            | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
	                    0xFF, 0, 0, 0, ray.Origin, 0.0, ray.Direction, 1e27f, packedPayload);
	reorderThreadWithHitObjectNV(hObj);
	hitObjectExecuteShaderNV(hObj, packedPayload);
#else
	Payload packedPayload = (Payload)0;
	// packedPayload.PackedDepth = bounceIndex & 0xFF;

	TraceRay(TLAS,
	         // RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
	         RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, packedPayload);
#endif

	return packedPayload;
}

float3 TracePath(inout RayDesc ray, inout uint seed)
{
	bool isInside = false;
	float3 throughput = 1.xxx;
	uint2 launchIndex = DispatchRaysIndex().xy;
	float3 color = 0.0.xxx;

	[loop] for (int bounceIndex = 0; bounceIndex < MAX_BOUNCES; ++bounceIndex)
	{
		// seed = tea(seed, bounceIndex); // Use the tea function to generate a new seed
		seed += bounceIndex;

		Payload payload = TraceRay(TLAS, ray, bounceIndex);

		ObjDesc objDesc = objDescs[payload.InstanceID];
		Material mat = materials[objDesc.MaterialIndex];
		float3 worldPosition = ray.Origin + ray.Direction * payload.HitT;
		PbrMaterial material = GetMaterialParams(ray, worldPosition, payload.WorldNormals, payload.BarycentricCoords, payload.HitT, payload.InstanceID, payload.PrimitiveIndex, launchIndex);
		

		const float3 view = -ray.Direction;
		// float2 iors;
		// if (isInside)
		// {
		// 	iors.x = mat.IOR;
		// 	iors.y = 1.0;
		// }
		// else
		// {
		// 	iors.x = 1.0;
		// 	iors.y = mat.IOR;
		// }

		// if (isInside)
		// {
		// 	payload.Normal = -payload.Normal;
		// }

		[branch] if (bounceIndex == 0)
		{
			o_ViewNormalsLuminance[launchIndex] = float4(mul((float3x3)(u_Camera.ViewMatrix), normalize(material.N)) * 0.5 + 0.5, 1.0);
			o_MetalnessRoughness[launchIndex] = float4(material.metallic, material.roughness.r, 0.0, 1.0);
			o_AlbedoColor[launchIndex] = float4(material.baseColor, material.opacity);
		}
		// o_PrimaryHitT[launchIndex] = float4(payload.HitT, 0.0, 0.0, 0.0);

		float NdotV = max(dot(material.N, view), 0.0);
		float3 F0 = lerp(Fdielectric.xxx, material.baseColor.rgb, material.metallic);

		// Miss, exit loop
		[branch] if (payload.HitT < 0.f)
		{
			color += EnvironmentSample(F0, ray.Direction, bounceIndex, material) * throughput;
		DebugImage[launchIndex] = float4(color, 1.0);

			return color;
		}

		bool thin_walled = (material.thickness == 0);
		if (isInside && !thin_walled)
		{
			const float3 abs_coeff = AbsorptionCoefficient(material.attenuationDistance, material.attenuationColor);
			throughput.x *= abs_coeff.x > 0.0 ? exp(-abs_coeff.x * payload.HitT) : 1.0;
			throughput.y *= abs_coeff.y > 0.0 ? exp(-abs_coeff.y * payload.HitT) : 1.0;
			throughput.z *= abs_coeff.z > 0.0 ? exp(-abs_coeff.z * payload.HitT) : 1.0;
		}

		color += DirectLighting(seed, F0, material, view, 1, TLAS) * throughput;
		color += material.emissive * throughput;


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
				// Continue path
				bool isSpecular = (sampleData.event_type & BSDF_EVENT_SPECULAR) != 0;
				bool isTransmission = (sampleData.event_type & BSDF_EVENT_TRANSMISSION) != 0;

				float3 offsetDir = dot(ray.Direction, material.N) > 0 ? material.N : -material.N;
				ray.Origin = offsetRay(material.WorldPosition, offsetDir);

				if (isTransmission)
				{
					isInside = !isInside;
				}
			}

			throughput *= min(0.9f, material.baseColor);
		}

		[branch] if (pushConst.EnableRussianRoulette)
		{
			float rrPcont = (max(throughput.x, max(throughput.y, throughput.z)) + 0.001F);

			if (RandomFloat(seed) > rrPcont) break; // Terminate the path

			throughput /= rrPcont; // Adjust throughput for Russian Roulette
		}

		// ray.TMin = 1e-4 * max(1.0f, length(ray.Direction));
		// ray.Origin = material.WorldPosition + ray.Direction * ray.TMin;
	}
	return color;
}

[shader("raygeneration")] void main()
{
	launchIndex = DispatchRaysIndex().xy;
	uint2 launchDimensions = DispatchRaysDimensions().xy;
	float4 previousColor = io_AccumulatedColor[launchIndex];

	uint seed = tea(launchIndex.y * launchDimensions.x + launchIndex.x, pushConst.FrameIndex * NBSAMPLES);
	// uint seed = initRNG(launchIndex, launchDimensions, pushConst.Frame, 0) * pushConst.Frame;

	o_ViewNormalsLuminance[launchIndex] = 0.0.xxxx;
	DebugImage[launchIndex] = 0.0.xxxx;
	float3 color = 0.0.xxx;

	[unroll] for (int smpl = 0; smpl < NBSAMPLES; smpl++)
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

	[branch] if (pushConst.PathtracingFrameIndex > 1)
	{
		color += previousColor.rgb;
		numPaths += previousColor.a;
	}

	io_AccumulatedColor[launchIndex] = float4(color, numPaths);
	o_Image[launchIndex] = float4(color * rcp(numPaths), 1.0f);
}

#pragma stage : ahit
#define BEY_SER 0

#include <Buffers.hlslh>
#include <Common.hlslh>
#include <HostDevice.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>
#include <Raytracing/Vertex.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;

[shader("anyhit")] void main(inout RayPackedPayload packedPayload, BuiltInTriangleIntersectionAttributes attrib)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDimensions = DispatchRaysDimensions().xy;

	uint seed = tea(launchIndex.y * launchDimensions.x + launchIndex.x, pushConst.FrameIndex);

	ObjDesc objResource = objDescs[InstanceID()];
	Material material = materials[objResource.MaterialIndex];

	float3 barycentrics = float3(1.0f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	float4 color = material.AlbedoColor;

	// [flatten] if (BEY_TEXURE_IS_TRANSPARENT(material.AlbedoTexIndex))
	{
		float2 uv = LoadInterpolatedVertexUVs(objResource, PrimitiveIndex(), barycentrics);
		color *= GetTex2D(material.AlbedoTexIndex).SampleLevel(GetBilinearWrapSampler(), uv, 0.0);
	}

	[branch] if (color.a < RandomFloat(seed))
	{
		IgnoreHit();
		return;
	}
}

#pragma stage : chit
#define BEY_SER 0

#include <Common.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>

#include <Buffers.hlslh>
#include <HostDevice.hlslh>
#include <Raytracing/BarycentricDerivatives.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Vertex.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;

static const float3 Fdielectric = 0.04;

[shader("closesthit")] void main(inout Payload payload, BuiltInTriangleIntersectionAttributes attrib)
{
	uint2 launchIndex = DispatchRaysIndex().xy;

	payload.HitT = RayTCurrent();
	payload.InstanceID = InstanceID();
	payload.PrimitiveIndex = PrimitiveIndex();	
	payload.BarycentricCoords = attrib.barycentrics;

	float3 normal = LoadInterpolatedVertexNormals(objDescs[InstanceID()], PrimitiveIndex(), float3(1.0f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y));
	payload.WorldNormals = PackNormal(normalize(mul((float3x3)(ObjectToWorld3x4()), normal)));
}

#pragma stage : miss
#define BEY_SER 0

#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>

[shader("miss")] 
void main(inout Payload payload)
{
	payload.HitT = -1.0f;
}