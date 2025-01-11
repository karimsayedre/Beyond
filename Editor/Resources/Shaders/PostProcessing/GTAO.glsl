///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2021, Intel Corporation 
// 
// SPDX-License-Identifier: MIT
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#version 450 core
#pragma stage : comp

const float PI = 3.141593f;
const float HALF_PI = 1.570796f;

// Global consts that need to be visible from shader
#define XE_GTAO_DEPTH_MIP_LEVELS                    6           // this one is hard-coded to 5 for now
#define XE_GTAO_NUMTHREADS_X                        16           // these can be changed
#define XE_GTAO_NUMTHREADS_Y                        16           // these can be changed
#define XE_GTAO_OCCLUSION_TERM_SCALE                (1.5f)		// for packing in UNORM (because raw, pre-denoised occlusion term can overshoot 1 but will later average out to 1)
#define XE_GTAO_FP32_DEPTHS 0

 // TODO: Set as compile time constants
 // Also set in SceneRenderer::Init()
#define XE_GTAO_COMPUTE_BENT_NORMALS 0



 layout(push_constant) uniform GTAOConstants
 {
    vec2 DepthUnpackConsts;
    vec2 CameraTanHalfFOV;

	vec2 NDCToViewMul;
    vec2 NDCToViewAdd;

	vec2 NDCToViewMul_x_PixelSize;
	float EffectRadius;                       // world (viewspace) maximum size of the shadow
	float EffectFalloffRange;

	float RadiusMultiplier;
	float FinalValuePower;
	float DenoiseBlurBeta;
	bool HalfRes;

	float SampleDistributionPower;
	float ThinOccluderCompensation;
	float DepthMIPSamplingOffset;
	int   NoiseIndex;                         // frameIndex % 64 if using TAA or 0 otherwise

	vec2 HZBUVFactor;
	float ShadowTolerance;
	float Padding;
} u_Settings;


layout(binding = 1) uniform sampler2D u_ViewNormal;
layout(binding = 2) uniform usampler2D u_HilbertLut;
layout(binding = 3) uniform sampler2D u_HiZDepth;

layout(binding = 4, r32ui) uniform writeonly uimage2D o_AOwBentNormals;
layout(binding = 5, r8) uniform writeonly image2D o_Edges;

vec2 SpatioTemporalNoise(uvec2 pixCoord, uint temporalIndex)    // without TAA, temporalIndex is always 0
{
	vec2 noise;
	// Hilbert curve driving R2 (see https://www.shadertoy.com/view/3tB3z3)
	uint index = uint(texelFetch(u_HilbertLut, ivec2(pixCoord % 64), 0).x);
	index += 288 * (temporalIndex % 64); // why 288? tried out a few and that's the best so far (with XE_HILBERT_LEVEL 6U) - but there's probably better :)
	// R2 sequence - see http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
	return vec2(fract(vec2(0.5) + vec2(index) * vec2(0.75487766624669276005, 0.5698402909980532659114)));
}


float LinearizeDepth(const float screenDepth)
{

}

vec4 LinearizeDepth(const vec4 screenDepths)
{
	return vec4(LinearizeDepth(screenDepths.x), LinearizeDepth(screenDepths.y), LinearizeDepth(screenDepths.z), LinearizeDepth(screenDepths.w)); 
}

vec4 XeGTAO_CalculateEdges(const float centerZ, const float leftZ, const float rightZ, const float topZ, const float bottomZ)
{
	vec4 edgesLRTB = vec4(leftZ, rightZ, topZ, bottomZ) - centerZ;

	float slopeLR = (edgesLRTB.y - edgesLRTB.x) * 0.5;
	float slopeTB = (edgesLRTB.w - edgesLRTB.z) * 0.5;
	vec4 edgesLRTBSlopeAdjusted = edgesLRTB + vec4(slopeLR, -slopeLR, slopeTB, -slopeTB);
	edgesLRTB = min(abs(edgesLRTB), abs(edgesLRTBSlopeAdjusted));
	return vec4(clamp((1.25f - edgesLRTB / (centerZ * 0.011)), 0.0f, 1.0f));
}

// Inputs are screen XY and viewspace depth, output is viewspace position
vec3 XeGTAO_ComputeViewspacePosition(const vec2 screenPos, const float viewspaceDepth)
{
    vec3 ret;
    ret.xy = (u_Settings.NDCToViewMul * screenPos + u_Settings.NDCToViewAdd) * viewspaceDepth;
    ret.z = viewspaceDepth;
    return ret;
}

// http://h14s.p5r.org/2012/09/0x5f3759df.html, [Drobot2014a] Low Level Optimizations for GCN, https://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf slide 63
float XeGTAO_FastSqrt(float x)
{
	return float((intBitsToFloat(0x1fbd1df5 + (floatBitsToInt(x) >> 1))));
}

// "Efficiently building a matrix to rotate one vector to another"
// http://cs.brown.edu/research/pubs/pdfs/1999/Moller-1999-EBA.pdf / https://dl.acm.org/doi/10.1080/10867651.1999.10487509
// (using https://github.com/assimp/assimp/blob/master/include/assimp/matrix3x3.inl#L275 as a code reference as it seems to be best)
mat3 XeGTAO_RotFromToMatrix(vec3 from, vec3 to)
{
    const float e       = dot(from, to);
    const float f       = abs(e); //(e < 0)? -e:e;

    // WARNING: This has not been tested/worked through, especially not for 16bit floats; seems to work in our special use case (from is always {0, 0, -1}) but wouldn't use it in general
    if(f > (1.0f - 0.0003f))
        return mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);

    const vec3 v      = cross(from, to);
    /* ... use this hand optimized version (9 mults less) */
    const float h       = (1.0) / (1.0 + e);      /* optimization by Gottfried Chen */
    const float hvx     = h * v.x;
    const float hvz     = h * v.z;
    const float hvxy    = hvx * v.y;
    const float hvxz    = hvx * v.z;
    const float hvyz    = hvz * v.y;

    mat3 mtx;
    mtx[0][0] = e + hvx * v.x;
    mtx[0][1] = hvxy - v.z;
    mtx[0][2] = hvxz + v.y;

    mtx[1][0] = hvxy + v.z;
    mtx[1][1] = e + h * v.y * v.y;
    mtx[1][2] = hvyz - v.x;

    mtx[2][0] = hvxz - v.y;
    mtx[2][1] = hvyz + v.x;
    mtx[2][2] = e + hvz * v.z;

    return mtx;
}

// input [-1, 1] and output [0, PI], from https://seblagarde.wordpress.com/2014/12/01/inverse-trigonometric-functions-gpu-optimization-for-amd-gcn-architecture/
float XeGTAO_FastACos(const float inX)
{ 
	float x = abs(inX); 
	float res = -0.156583f * x + HALF_PI; 
	res *= XeGTAO_FastSqrt(1.0 - x); 
	return (inX >= 0) ? res : PI - res; 
}

// packing/unpacking for edges; 2 bits per edge mean 4 gradient values (0, 0.33, 0.66, 1) for smoother transitions!
float XeGTAO_PackEdges(vec4 edgesLRTB)
{
    edgesLRTB = round(clamp(edgesLRTB, 0.0, 1.0) * 2.9);
    return dot(vec4(edgesLRTB), vec4(64.0 / 255.0, 16.0 / 255.0, 4.0 / 255.0, 1.0 / 255.0));
}

uint XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4 unpackedInput)
{
    return ((uint(clamp(unpackedInput.x, 0.0, 1.0) * 255.f + 0.5f)) |
            (uint(clamp(unpackedInput.y, 0.0, 1.0) * 255.f + 0.5f) << 8 ) |
            (uint(clamp(unpackedInput.z, 0.0, 1.0) * 255.f + 0.5f) << 16 ) |
            (uint(clamp(unpackedInput.w, 0.0, 1.0) * 255.f + 0.5f) << 24 ) );
}

uint XeGTAO_EncodeVisibilityBentNormal(float visibility, vec3 bentNormal)
{
    return XeGTAO_FLOAT4_to_R8G8B8A8_UNORM(vec4(bentNormal * 0.5 + 0.5, visibility));
}

void XeGTAO_MainPass(const ivec2 outputPixCoord, const ivec2 inputPixCoords, float sliceCount, float stepsPerSlice, const vec2 localNoise)
{

	const vec4 viewspaceNormalLuminance = texelFetch(u_ViewNormal, inputPixCoords, 0);
	vec3 viewspaceNormal;
	viewspaceNormal.x = viewspaceNormalLuminance.x;
	viewspaceNormal.yz = -viewspaceNormalLuminance.yz;

	vec2 normalizedScreenPos = inputPixCoords * u_InvFullResolution;

    vec4 valuesUL = LinearizeDepth(textureGather(u_HiZDepth, vec2(normalizedScreenPos * u_Settings.HZBUVFactor)));
    vec4 valuesBR = LinearizeDepth(textureGatherOffset(u_HiZDepth, vec2(normalizedScreenPos * u_Settings.HZBUVFactor), ivec2(1, 1)));

	// viewspace Z at the center
	float viewspaceZ  = valuesUL.y; //sourceViewspaceDepth.SampleLevel( depthSampler, normalizedScreenPos, 0 ).x; 

	// viewspace Zs left top right bottom
	const float pixLZ = valuesUL.x;
	const float pixTZ = valuesUL.z;
	const float pixRZ = valuesBR.z;
	const float pixBZ = valuesBR.x;

	vec4 edgesLRTB  = XeGTAO_CalculateEdges(viewspaceZ, pixLZ, pixRZ, pixTZ, pixBZ);
	imageStore(o_Edges, outputPixCoord, XeGTAO_PackEdges(edgesLRTB).xxxx);

	// Move center pixel slightly towards camera to avoid imprecision artifacts due to depth buffer imprecision; offset depends on depth texture format used
#if XE_GTAO_FP32_DEPTHS
	viewspaceZ *= 0.99999;     // this is good for FP32 depth buffer
#else
	viewspaceZ *= 0.99920;     // this is good for FP16 depth buffer
#endif

	vec3 pixCenterPos = XeGTAO_ComputeViewspacePosition(normalizedScreenPos, viewspaceZ);
	const vec3 viewVec = normalize(-pixCenterPos);

	// prevents normals that are facing away from the view vector - xeGTAO struggles with extreme cases, but in Vanilla it seems rare so it's disabled by default
	viewspaceNormal = normalize(viewspaceNormal + max(0, -dot(viewspaceNormal, viewVec)) * viewVec);

	const float effectRadius              = u_Settings.EffectRadius * u_Settings.RadiusMultiplier;
	const float sampleDistributionPower   = u_Settings.SampleDistributionPower;
	const float thinOccluderCompensation  = u_Settings.ThinOccluderCompensation;
	const float falloffRange              = u_Settings.EffectFalloffRange * effectRadius;

	const float falloffFrom       = effectRadius * (1.0f - u_Settings.EffectFalloffRange);

	// fadeout precompute optimisation
	const float falloffMul        = -1.0f / (falloffRange);
	const float falloffAdd        = falloffFrom / (falloffRange) + 1.0f;

	float visibility = 0.f;
#if XE_GTAO_COMPUTE_BENT_NORMALS
	vec3 bentNormal = vec3(0.0);
#else
	vec3 bentNormal = viewspaceNormal;
#endif

	// see "Algorithm 1" in https://www.activision.com/cdn/research/Practical_Real_Time_Strategies_for_Accurate_Indirect_Occlusion_NEW%20VERSION_COLOR.pdf
	{
		const float noiseSlice  = localNoise.x;
		const float noiseSample = localNoise.y;

		// quality settings / tweaks / hacks
		const float pixelTooCloseThreshold = 1.3;      // if the offset is under approx pixel size (pixelTooCloseThreshold), push it out to the minimum distance

		// approx viewspace pixel size at pixCoord; approximation of NDCToViewspace( normalizedScreenPos.xy + consts.ViewportPixelSize.xy, pixCenterPos.z ).xy - pixCenterPos.xy;
		const vec2 pixelDirRBViewspaceSizeAtCenterZ = viewspaceZ.xx * u_Settings.NDCToViewMul_x_PixelSize;

		float screenspaceRadius = effectRadius / pixelDirRBViewspaceSizeAtCenterZ.x;

		// fade out for small screen radii 
		visibility += clamp((10 - screenspaceRadius) / 100, 0.0, 1.0) * 0.5;

#if 1   // sensible early-out for even more performance, also doesn't occlude skybox. Not tested in original implementation.
		// Also when surface is NOT in shadow. This might cause huge differences between frame times. 
		if(screenspaceRadius < pixelTooCloseThreshold || isnan(viewspaceNormal.x)/* || viewspaceNormalLuminance.a < 0.5 */)
		{
		#if XE_GTAO_COMPUTE_BENT_NORMALS
			imageStore(o_AOwBentNormals, outputPixCoord, XeGTAO_EncodeVisibilityBentNormal(1.0, bentNormal).xxxx);
		#else
			imageStore(o_AOwBentNormals, outputPixCoord, uint(255).xxxx);
		#endif
			return;
		}
#endif

		// this is the min distance to start sampling from to avoid sampling from the center pixel (no useful data obtained from sampling center pixel)
		const float minS = pixelTooCloseThreshold / screenspaceRadius;

		for(float slice = 0; slice < sliceCount; slice++)
		{
			float sliceK = (slice + noiseSlice) / sliceCount;
			// lines 5, 6 from the paper
			float phi = sliceK * PI;
			float cosPhi = cos(phi);
			float sinPhi = sin(phi);
			vec2 omega = vec2(cosPhi, -sinPhi);       //vec2 on omega causes issues with big radii

			// convert to screen units (pixels) for later use
			omega *= screenspaceRadius;

			// line 8 from the paper
			const vec3 directionVec = vec3(cosPhi, sinPhi, 0);

			// line 9 from the paper
			const vec3 orthoDirectionVec = directionVec - (dot(directionVec, viewVec) * viewVec);

			// line 10 from the paper
			//axisVec is orthogonal to directionVec and viewVec, used to define projectedNormal
			const vec3 axisVec = normalize(cross(orthoDirectionVec, viewVec));

			// alternative line 9 from the paper
			// vec3 orthoDirectionVec = cross( viewVec, axisVec );

			// line 11 from the paper
			vec3 projectedNormalVec = viewspaceNormal - axisVec * dot(viewspaceNormal, axisVec);

			// line 13 from the paper
			float signNorm = sign(dot(orthoDirectionVec, projectedNormalVec));

			// line 14 from the paper
			float projectedNormalVecLength = length(projectedNormalVec);
			float cosNorm = clamp(dot(projectedNormalVec, viewVec) / projectedNormalVecLength, 0.0f, 1.0f);

			// line 15 from the paper
			float n = signNorm * XeGTAO_FastACos(cosNorm);

			// this is a lower weight target; not using -1 as in the original paper because it is under horizon, so a 'weight' has different meaning based on the normal
			const float lowHorizonCos0  = cos(n + HALF_PI);
			const float lowHorizonCos1  = cos(n - HALF_PI);

			// lines 17, 18 from the paper, manually unrolled the 'side' loop
			float horizonCos0           = lowHorizonCos0; //-1;
			float horizonCos1           = lowHorizonCos1; //-1;

			for(float step = 0; step < stepsPerSlice; step++)
			{
				// R1 sequence (http://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/)
				const float stepBaseNoise = float(slice + step * stepsPerSlice) * 0.6180339887498948482; // <- this should unroll
				float stepNoise = fract(noiseSample + stepBaseNoise);

				// approx line 20 from the paper, with added noise
				float s = (step + stepNoise) / (stepsPerSlice); // + vec2(1e-6f));

				// additional distribution modifier
				s       = pow(s, sampleDistributionPower);

				// avoid sampling center pixel
				s       += minS;

				// approx lines 21-22 from the paper, unrolled
				vec2 sampleOffset = s * omega;

				float sampleOffsetLength = length(sampleOffset);

				// note: when sampling, using point_point_point or point_point_linear sampler works, but linear_linear_linear will cause unwanted interpolation between neighbouring depth values on the same MIP level!
				const float mipLevel    = clamp(log2(sampleOffsetLength) - u_Settings.DepthMIPSamplingOffset, 0, XE_GTAO_DEPTH_MIP_LEVELS);

				// Snap to pixel center (more correct direction math, avoids artifacts due to sampling pos not matching depth texel center - messes up slope - but adds other 
				// artifacts due to them being pushed off the slice). Also use full precision for high res cases.
				sampleOffset = round(sampleOffset) * u_InvFullResolution;


				vec2 sampleScreenPos0 = normalizedScreenPos + sampleOffset;
				float  SZ0 = LinearizeDepth(textureLod(u_HiZDepth, sampleScreenPos0 * u_Settings.HZBUVFactor, mipLevel).x);
				vec3  samplePos0 = XeGTAO_ComputeViewspacePosition(sampleScreenPos0, SZ0);

				vec2 sampleScreenPos1 = normalizedScreenPos - sampleOffset;
				float  SZ1 = LinearizeDepth(textureLod(u_HiZDepth, sampleScreenPos1 * u_Settings.HZBUVFactor, mipLevel).x);
				vec3 samplePos1 = XeGTAO_ComputeViewspacePosition(sampleScreenPos1, SZ1);

				vec3 sampleDelta0     = (samplePos0 - vec3(pixCenterPos)); // using lpfloat for sampleDelta causes precision issues
				vec3 sampleDelta1     = (samplePos1 - vec3(pixCenterPos)); // using lpfloat for sampleDelta causes precision issues
				float sampleDist0     = length(sampleDelta0);
				float sampleDist1     = length(sampleDelta1);

				// approx lines 23, 24 from the paper, unrolled
				vec3 sampleHorizonVec0 = (sampleDelta0 / sampleDist0);
				vec3 sampleHorizonVec1 = (sampleDelta1 / sampleDist1);

				// this is our own thickness heuristic that relies on sooner discarding samples behind the center
				float falloffBase0    = length(vec3(sampleDelta0.x, sampleDelta0.y, sampleDelta0.z * (1 + thinOccluderCompensation)));
				float falloffBase1    = length(vec3(sampleDelta1.x, sampleDelta1.y, sampleDelta1.z * (1 + thinOccluderCompensation)));
				float weight0         = clamp(falloffBase0 * falloffMul + falloffAdd, 0.0f, 1.0f);
				float weight1         = clamp(falloffBase1 * falloffMul + falloffAdd, 0.0f, 1.0f);

				// sample horizon cos
				float shc0 = dot(sampleHorizonVec0, viewVec);
				float shc1 = dot(sampleHorizonVec1, viewVec);

				// discard unwanted samples
				shc0 = mix(lowHorizonCos0, shc0, weight0); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos0), acos(shc0), weight0 ));
				shc1 = mix(lowHorizonCos1, shc1, weight1); // this would be more correct but too expensive: cos(lerp( acos(lowHorizonCos1), acos(shc1), weight1 ));



				// thickness heuristic - see "4.3 Implementation details, Height-field assumption considerations"
#if 0   // (disabled, not used) this should match the paper
				lpfloat newhorizonCos0 = max(horizonCos0, shc0);
				lpfloat newhorizonCos1 = max(horizonCos1, shc1);
				horizonCos0 = (horizonCos0 > shc0) ? (mix(newhorizonCos0, shc0, thinOccluderCompensation)) : (newhorizonCos0);
				horizonCos1 = (horizonCos1 > shc1) ? (mix(newhorizonCos1, shc1, thinOccluderCompensation)) : (newhorizonCos1);
#elif 0 // (disabled, not used) this is slightly different from the paper but cheaper and provides very similar results
				horizonCos0 = mix(max(horizonCos0, shc0), shc0, thinOccluderCompensation);
				horizonCos1 = mix(max(horizonCos1, shc1), shc1, thinOccluderCompensation);
#else   // this is a version where thicknessHeuristic is completely disabled
				horizonCos0 = max(horizonCos0, shc0);
				horizonCos1 = max(horizonCos1, shc1);
#endif
			}


#if 1       // I can't figure out the slight overdarkening on high slopes, so I'm adding this fudge - in the training set, 0.05 is close (PSNR 21.34) to disabled (PSNR 21.45)
			projectedNormalVecLength = mix(projectedNormalVecLength, 1.0, 0.05);
#endif

			// line ~27, unrolled
			float h0 = -XeGTAO_FastACos(horizonCos1);
			float h1 = XeGTAO_FastACos(horizonCos0);
#if 0       // we can skip clamping for a tiny little bit more performance
			h0 = n + clamp(h0 - n, -HALF_PI, HALF_PI);
			h1 = n + clamp(h1 - n, -HALF_PI, HALF_PI);
#endif
			float iarc0 = (cosNorm + 2.0f * h0 * sin(n) - cos(2.0 * h0 - n)) / 4.0;
			float iarc1 = (cosNorm + 2.0f * h1 * sin(n) - cos(2.0 * h1 - n)) / 4.0;
			float localVisibility = projectedNormalVecLength * (iarc0 + iarc1);
			visibility += localVisibility;


#if XE_GTAO_COMPUTE_BENT_NORMALS
			// see "Algorithm 2 Extension that computes bent normals b."
			float t0 = (6 * sin(h0 - n) - sin(3 * h0 - n) + 6 * sin(h1 - n) - sin(3 * h1 - n) + 16 * sin(n) - 3 * (sin(h0 + n) + sin(h1 + n))) / 12;
			float t1 = (-cos(3 * h0 - n) - cos(3 * h1 - n) + 8 * cos(n) - 3 * (cos(h0 + n) + cos(h1 + n))) / 12;
			vec3 localBentNormal = vec3(directionVec.x * t0, directionVec.y * t0, -float(t1));
			localBentNormal = XeGTAO_RotFromToMatrix(vec3(0,0, -1), viewVec) * localBentNormal * projectedNormalVecLength;
			bentNormal += localBentNormal;
#endif
		}
		visibility /= float(sliceCount);
		visibility = pow(visibility, u_Settings.FinalValuePower * mix(1.0f, u_Settings.ShadowTolerance, viewspaceNormalLuminance.a));
		visibility = max(0.03, visibility); // disallow total occlusion (which wouldn't make any sense anyhow since pixel is visible but also helps with packing bent normals)

#if XE_GTAO_COMPUTE_BENT_NORMALS
		bentNormal = normalize(bentNormal);
#endif
	}

	visibility = clamp(visibility / float(XE_GTAO_OCCLUSION_TERM_SCALE), 0.0, 1.0);

	#if XE_GTAO_COMPUTE_BENT_NORMALS
		imageStore(o_AOwBentNormals, outputPixCoord, XeGTAO_EncodeVisibilityBentNormal(visibility, bentNormal).xxxx);
	#else
		imageStore(o_AOwBentNormals, outputPixCoord, uint(visibility * 255.0 + 0.5).xxxx);
	#endif

}


layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main()
{
	const ivec2 outputPixCoords = ivec2(gl_GlobalInvocationID);
	const ivec2 inputPixCoords = outputPixCoords * (1 + int(u_Settings.HalfRes));

	XeGTAO_MainPass(outputPixCoords, inputPixCoords, 9, 3, SpatioTemporalNoise(inputPixCoords, u_Settings.NoiseIndex));
}


