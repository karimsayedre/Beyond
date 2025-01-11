#pragma stage : rgen

static uint2 launchIndex;

#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>
#include <Raytracing/Lighting.hlslh>

#include <Raytracing/RaytracingMath.hlslh>
VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;

static const int NBSAMPLES = 1;
[shader("raygeneration")] void main()
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDimensions = DispatchRaysDimensions().xy;

	// Initialize the random number
	uint seed = tea(launchIndex.y * launchDimensions.x + launchDimensions.x, 1);
	// prd.Seed = seed;

	o_AlbedoColor[launchIndex] = 0.0f.xxxx;
	o_MetalnessRoughness[launchIndex] = 0.0f.xxxx;
	o_ViewNormalsLuminance[launchIndex] = 0.0f.xxxx;
	DebugImage[launchIndex] = 0.0f.xxxx;

	float3 hitValues = 0.0f.xxx;

	for (int smpl = 0; smpl < NBSAMPLES; smpl++)
	{
		float r1 = rnd(seed);
		float r2 = rnd(seed);
		// Subpixel jitter: send the ray through a different position inside the
		// pixel each time, to provide antialiasing.
		float2 subpixel_jitter = pushConst.FrameIndex == 0 ? float2(0.5f, 0.5f) : float2(r1, r2);

		const float2 pixelCenter = float2(launchIndex.xy) + subpixel_jitter;
		const float2 inUV = pixelCenter / float2(launchDimensions.xy);

		float2 d = inUV * 2.0 - 1.0;
		float4 origin = mul(u_Camera.InverseViewMatrix, float4(0, 0, 0, 1));
		float4 target = mul(u_Camera.InverseProjectionMatrix, float4(d.x, d.y, 1, 1));
		float4 direction = mul(u_Camera.InverseViewMatrix, float4(normalize(target.xyz), 0));

		RayDesc ray;
		ray.TMin = 0.f;
		ray.TMax = 1e27f;
		ray.Origin = origin.xyz;
		ray.Direction = direction.xyz;

		Payload payload = (Payload)0;
		const float3 view = -ray.Direction;

		{
			TraceRay(TLAS, RAY_FLAG_CULL_BACK_FACING_TRIANGLES, 0xFF, 0, 0, 0, ray, payload);
			ObjDesc objDesc = objDescs[payload.InstanceID];
			Material mat = materials[objDesc.MaterialIndex];
			float3 worldPosition = ray.Origin + ray.Direction * payload.HitT;
			PbrMaterial material = GetMaterialParams(ray, worldPosition, payload.WorldNormals, payload.BarycentricCoords, payload.HitT, payload.InstanceID, payload.PrimitiveIndex, launchIndex);
			float NdotV = max(dot(material.N, view), 0.0);
			float3 F0 = lerp(0.04f.xxx, material.baseColor.rgb, material.metallic);
			hitValues += IBL(F0, ray.Direction, 0.0, material.baseColor.xyz, material.N, material.roughness.r, material.metallic, max(dot(material.Ng, ray.Direction), 0.0)) * u_SceneData.EnvironmentMapIntensity;


			// DebugImage[launchIndex] = float4(material.baseColor.rgb, 1.0f);
		
			hitValues += material.emissive;
			hitValues += DirectLighting(seed, F0, material, view, 1, TLAS, 0.0f);
		}

		// if (payload.HitT < 0.0f) hitValues += u_SkyboxTexture.SampleLevel(GetBilinearWrapSampler(), ray.Direction, u_SceneData.EnvironmentMapLod).rgb * u_SceneData.EnvironmentMapIntensity;
	}

	hitValues = hitValues / NBSAMPLES;

	if (true)
	{
		// First frame, replace the value in the buffer
		o_Image[launchIndex] = float4(hitValues, 1.f);
	}
	else
	{
		// Do accumulation over time
		float a = 1.0f / float(pushConst.FrameIndex + 1);
		float3 old_color = o_Image.Load(launchIndex).xyz;
		o_Image[launchIndex] = float4(lerp(old_color, hitValues, a), 1.f);

		// imageStore(o_Image, int2(gl_LaunchIDEXT.xy), float4(hitValues, 1.f));
	}
}

#pragma stage : chit

#include <Common.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>
#include <HostDevice.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/Vertex.hlslh>
#include <Buffers.hlslh>
#include <PBR.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/RaytracingMath.hlslh>

// VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;
static const float3 Fdielectric = 0.04;

[shader("closesthit")] void main(inout Payload payload, BuiltInTriangleIntersectionAttributes attrib)
{
	uint2 launchIndex = DispatchRaysIndex().xy;

	payload = (Payload)0;
	payload.HitT = RayTCurrent();
	payload.InstanceID = InstanceID();
	payload.PrimitiveIndex = PrimitiveIndex();	
	payload.BarycentricCoords = attrib.barycentrics;

	float3 normal = LoadInterpolatedVertexNormals(objDescs[InstanceID()], PrimitiveIndex(), float3(1.0f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y));
	payload.WorldNormals = PackNormal(normalize(mul((float3x3)(ObjectToWorld3x4()), normal)));
}

#pragma stage : miss

#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>

        [shader("miss")] void main(inout Payload payload)
{
	payload.HitT = -1.0f;
}
