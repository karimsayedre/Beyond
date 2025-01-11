#pragma stage : rgen

// Enabling this would need a custom version of spirv-cross core library, 
// it's commented in Dependencies.lua in root directory. 
// Also make sure to enable it in all stages just in case.
#define BEY_SER 0 

static uint2 launchIndex;

#include <Common.hlslh>
#include <SER.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Lighting.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RaytracingMath.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;
// [[vk::binding(12, 1)]] RWTexture2D<float4> o_MotionVectors : register(u1);

static const int NBSAMPLES = 1;
static const float Fdielectric = 0.04f;
static const int MAX_BOUNCES = 15;
static const float MIN_THROUGHPUT = 0.005f;

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// Polynomial approximation by Christophe Schlick
float Schlick(const float cosine, const float refractionIndex)
{
	float r0 = (1 - refractionIndex) / (1 + refractionIndex);
	r0 *= r0;
	return r0 + (1 - r0) * pow(1 - cosine, 5);
}

// Main function to compute the next ray direction
float3 ComputeNextRayDirection(inout uint seed, float3 v, float3 h, float roughness, out bool isSpecular)
{
	float3 l;

	// 50% chance for diffuse vs. specular reflection
	if (RandomFloat(seed) < roughness)
	{
		// Diffuse reflection: Sample a random direction in the hemisphere
		l = SampleHemisphereCosine(RandomFloat2(seed), h);
		isSpecular = false;
	}
	else
	{
		// Specular reflection: Use the GGX model to calculate the half-vector and reflected direction
		l = 2.0 * dot(v, h) * h - v; // Reflect the view direction around the half-vector
		isSpecular = true;
		// Ensure the reflection direction is above the surface
		// if (dot(l, h) <= 0.0f)
		// {
		// 	return float3(0.0, 0.0, 0.0); // Return zero if the direction is invalid
		// }
	}

	return normalize(l);
}

float3 ApplyTransmissionAttenuation(float3 transmittance, float distance, float3 attenuationColor)
{
	return transmittance * exp(-attenuationColor * distance);
}

float ReflectionProbability(float3 view,     // Incoming light direction
                            float3 H,        // Half-vector between light and view
                            float ior,       // Index of refraction of the material
                            float roughness, // roughness of the material (microfacet scattering)
                            float NdotV,     // Dot product of normal and view direction
                            float alpha      // Opacity (1 = opaque, 0 = fully transparent)
)
{

	// Determine if the ray is exiting the object and adjust H accordingly
	// bool isExiting = NdotV < 0.0;
	// float3 adjustedH = isExiting ? -H : H;
	float cosTheta = dot(view, H);
	float fresnelReflectance = Schlick(cosTheta, ior);
	// Modulate reflectance based on roughness, which affects microfacet distribution
	fresnelReflectance += roughness * (1.0 - fresnelReflectance);

	// Adjust reflectance probability with opacity; more transparent materials favor transmission
	float transmissionProb = (1.0 - fresnelReflectance) * (1.0 - alpha);

	// Final reflection probability, balancing Fresnel and transparency
	return lerp(fresnelReflectance * transmissionProb, 1.0, alpha);
}

Payload TraceRay(RaytracingAccelerationStructure TLAS, RayDesc ray, uint bounceIndex)
{
#if BEY_SER
	payload = (Payload)0;

	createHitObjectNV();

	HitObjectNV hObj;
	hitObjectRecordEmptyNV(hObj);
	hitObjectTraceRayNV(hObj, TLAS,
	                    RAY_FLAG_NONE // RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
	                            | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES,
	                    0xFF, 0, 0, 0, ray.Origin, 0.0, ray.Direction, 1e27f, payload);
	reorderThreadWithHitObjectNV(hObj);
	hitObjectExecuteShaderNV(hObj, payload);
#else
	Payload payload = (Payload)0;

	TraceRay(TLAS,
	         // RAY_FLAG_CULL_BACK_FACING_TRIANGLES,
	         RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES | RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);
#endif

	return payload;
}

float3 TracePath(inout RayDesc ray, inout uint seed)
{
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



		// DebugImage[launchIndex] = float4(, 1.0);

		const float3 view = -ray.Direction;
		// float2 iors;
		// if (dot(payload.Normal, view) < 0.0)
		// {
		// 	iors.x = mat.IOR;
		// 	iors.y = 1.0;  

		// } 
		// else
		// {
		// 	iors.x = 1.0;
		// 	iors.y = mat.IOR;
		// }

		// float eta = iors.x / iors.y;

		// if (dot(material.Normal, view) < 0.0)
		// {
		// 	material.Normal = -material.Normal;
		// }

		// float4 modelMatrix[3] = r_Transforms.Load(0).Transform[objDesc.MaterialIndex].ModelMatrix;
		// float4x4 transform = (float4x4(float4(modelMatrix[0].x, modelMatrix[1].x, modelMatrix[2].x, 0.0), 
		// 							   float4(modelMatrix[0].y, modelMatrix[1].y, modelMatrix[2].y, 0.0),
		// 							   float4(modelMatrix[0].z, modelMatrix[1].z, modelMatrix[2].z, 0.0), 
		// 							   float4(modelMatrix[0].w, modelMatrix[1].w, modelMatrix[2].w, 1.0)));

		
		float3 worldPosition = ray.Origin + ray.Direction * payload.HitT;
		PbrMaterial material = GetMaterialParams(ray, worldPosition, payload.WorldNormals, payload.BarycentricCoords, payload.HitT, payload.InstanceID, payload.PrimitiveIndex, launchIndex);
		
		[branch] if (bounceIndex == 0)
		{
			o_ViewNormalsLuminance[launchIndex] = float4(mul((float3x3)(u_Camera.ViewMatrix), normalize(material.N)) * 0.5 + 0.5, 1.0);
			o_MetalnessRoughness[launchIndex] = float4(material.metallic, material.roughness.x, 0.0, 1.0);
			o_AlbedoColor[launchIndex] = float4(material.baseColor, material.opacity);
		}
		o_PrimaryHitT[launchIndex] = float4(material.HitT, 0.0, 0.0, 0.0);

		float NdotV = max(dot(material.N, view), 0.0);
		float3 F0 = lerp(Fdielectric.xxx, material.baseColor.rgb, material.metallic);
		// float3 F = F_Schlick(F0, dot(material.Normal, view));

		// Miss, exit loop
		[branch] if (material.HitT < 0.f)
		{
			color += EnvironmentSample(F0, ray.Direction, bounceIndex, material) * throughput;
			return color;
		}

		if (dot(material.N, view) < 0.0)
		{
			const float3 abs_coeff = AbsorptionCoefficient(material.attenuationDistance, material.baseColor.rgb);
			throughput.x *= abs_coeff.x > 0.0 ? exp(-abs_coeff.x * material.HitT) : 1.0;
			throughput.y *= abs_coeff.y > 0.0 ? exp(-abs_coeff.y * material.HitT) : 1.0;
			throughput.z *= abs_coeff.z > 0.0 ? exp(-abs_coeff.z * material.HitT) : 1.0;
		}

		color += material.emissive * throughput;
		color += DirectLighting(seed, F0, material, view, 1, TLAS) * throughput;

		float3 H = ImportanceSampleGGX(RandomFloat2(seed), material.roughness.x, material.N);

		float3 bsdf = 0.0.xxx;

		float3 L;
		// bool transmitSuccess = Refract(ray.Direction, H, eta, L);
		// float reflectProb = ReflectionProbability(view, H, eta, material.roughness, NdotV, material.Albedo.a);

		// if (!transmitSuccess || RandomFloat(seed) < reflectProb)
		if (true)
		// if (false)
		{ // Reflection: Calculate the half-vector and reflected direction

			bool isSpecular;
			ray.Direction = ComputeNextRayDirection(seed, view, H, material.roughness.x, isSpecular);

			// bsdf = BRDF_GGX(H, view, ray.Direction, F0, material.Albedo.rgb, material.roughness, material.metallic);
			// bsdf = EvaluateBRDF(view, ray.Direction, material.Normal, H, material.roughness, material.metallic, material.Albedo.rgb, F, isSpecular);
			// float alpha = material.roughness * material.roughness;

			// float pdf = (alpha * alpha) / (PI * pow((NdotV * NdotV) * (alpha * alpha - 1.0) + 1.0, 2.0));

			// float NdotL = max(dot(material.Normal, ray.Direction), 0.0);
			// bsdf = material.Albedo.rgb * (bsdf * NdotL) / max(pdf, 0.01);
			// throughput += bsdf * throughput;

			throughput *= min(material.baseColor.rgb, 0.9.xxx);
		}
		else
		{
			// Refraction: Calculate the refracted direction and Fresnel effect

			ray.Direction = L;

			color *= ApplyTransmissionAttenuation(bsdf, material.HitT, material.baseColor.rgb);
			float NdotL = max(dot(material.N, ray.Direction), 0.0);
			// throughput *= (bsdf); // / max(pdf, 0.01);
			// color += bsdf * throughput;

			// if (bounceIndex == 0)
			// USE BTDF HERE
		}

		// color += bsdf * throughput;

		[branch] if (pushConst.EnableRussianRoulette)
		{
			float rrPcont = (max(throughput.x, max(throughput.y, throughput.z)) + 0.001F);

			if (RandomFloat(seed) > rrPcont) break; // Terminate the path

			throughput /= rrPcont; // Adjust throughput for Russian Roulette
		}

		ray.TMin = 1e-4 * max(1.0f, length(ray.Direction));
		ray.Origin = material.WorldPosition + ray.Direction * ray.TMin;
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
	o_AlbedoColor[launchIndex] = 0.0.xxxx;
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

#include <Common.slh>
#include <Common.hlslh>
#include <Buffers.hlslh>
#include <HostDevice.hlslh>
#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/Random.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>
#include <Raytracing/Vertex.hlslh>

VK_PUSH_CONST ConstantBuffer<RaytracingPushConst> pushConst;

[shader("anyhit")] void main(inout Payload packedPayload, BuiltInTriangleIntersectionAttributes attrib)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	uint2 launchDimensions = DispatchRaysDimensions().xy;

	ObjDesc objResource = objDescs[InstanceID()];
	Material material = materials[objResource.MaterialIndex];

	float3 barycentrics = float3(1.0f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	float4 color = material.AlbedoColor;

	[flatten] if (BEY_TEXURE_IS_TRANSPARENT(material.AlbedoTexIndex))
	{
		float2 uv = LoadInterpolatedVertexUVs(objResource, PrimitiveIndex(), barycentrics);
		color *= GetTex2D(material.AlbedoTexIndex).SampleLevel(GetBilinearWrapSampler(), uv, 0.0);
	}

	[branch] if (color.a < 0.001f)
	{
		IgnoreHit();
		return;
	}

	uint seed = tea(launchIndex.y * launchDimensions.x + launchIndex.x, pushConst.PathtracingFrameIndex);
	float randValue = rnd(seed);

	[branch]
	if (color.a < randValue) {
	    IgnoreHit();
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
#define BEY_SER 0

#include <Raytracing/Descriptors.hlslh>
#include <Raytracing/RaytracingPayload.hlslh>

        [shader("miss")] void main(inout Payload packedPayload)
{
	packedPayload.HitT = -1.0f;
}