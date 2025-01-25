// -- Beyond Engine PBR shader --
// -----------------------------
// Note: this shader is still very much in progress. There are likely many bugs and future additions that will go in.
//       Currently heavily updated.
//
// References upon which this is based:
// - Unreal Engine 4 PBR notes
// (https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf)
// - Frostbite's SIGGRAPH 2014 paper
// (https://seblagarde.wordpress.com/2015/07/14/siggraph-2014-moving-frostbite-to-physically-based-rendering/)
// - Michał Siejak's PBR project (https://github.com/Nadrin)
// - Cherno's implementation from years ago in the Sparky engine (https://github.com/TheCherno/Sparky)

#version 450 core
#pragma stage : vert

#include <Buffers.glslh>
#include <Lighting.glslh>
#include <ShadowMapping.glslh>

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec3 a_Tangent;
layout(location = 3) in vec3 a_Binormal;
layout(location = 4) in vec2 a_TexCoord;

layout(push_constant) uniform Uniform
{
	uint u_DrawIndex;
};

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	vec3 ShadowMapCoords[4];
	vec3 ViewPosition;
};

layout(location = 0) out VertexOutput Output;

// Make sure both shaders compute the exact same answer(PreDepth).
// We need to have the same exact calculations to produce the gl_Position value (eg. matrix multiplications).
invariant gl_Position;

void main()
{
	vec4 modelMatrix[3] = r_Transforms.Transform[u_DrawIndex + gl_InstanceIndex].ModelMatrix;
	mat4 transform = mat4(vec4(modelMatrix[0].x, modelMatrix[1].x, modelMatrix[2].x, 0.0), vec4(modelMatrix[0].y, modelMatrix[1].y, modelMatrix[2].y, 0.0),
	                      vec4(modelMatrix[0].z, modelMatrix[1].z, modelMatrix[2].z, 0.0), vec4(modelMatrix[0].w, modelMatrix[1].w, modelMatrix[2].w, 1.0));

	vec4 worldPosition = transform * vec4(a_Position, 1.0);

	Output.WorldPosition = worldPosition.xyz;
	Output.Normal = mat3(transform) * a_Normal;
	Output.TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
	Output.WorldNormals = mat3(transform) * mat3(a_Tangent, a_Binormal, a_Normal);
	Output.WorldTransform = mat3(transform);
	Output.Binormal = a_Binormal;

	vec4 shadowCoords[4];
	shadowCoords[0] = u_DirShadow.DirLightMatrices[0] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[1] = u_DirShadow.DirLightMatrices[1] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[2] = u_DirShadow.DirLightMatrices[2] * vec4(Output.WorldPosition.xyz, 1.0);
	shadowCoords[3] = u_DirShadow.DirLightMatrices[3] * vec4(Output.WorldPosition.xyz, 1.0);
	Output.ShadowMapCoords[0] = vec3(shadowCoords[0].xyz / shadowCoords[0].w);
	Output.ShadowMapCoords[1] = vec3(shadowCoords[1].xyz / shadowCoords[1].w);
	Output.ShadowMapCoords[2] = vec3(shadowCoords[2].xyz / shadowCoords[2].w);
	Output.ShadowMapCoords[3] = vec3(shadowCoords[3].xyz / shadowCoords[3].w);

	Output.ViewPosition = vec3(u_Camera.ViewMatrix * vec4(Output.WorldPosition, 1.0));

	gl_Position = u_Camera.ViewProjectionMatrix * worldPosition;
}

#version 450 core

#pragma stage : frag

#include <Buffers.glslh>
#include <Lighting.glslh>
#include <PBR.glslh>
#include <PBR_Resources.glslh>
#include <ShadowMapping.glslh>

// Constant normal incidence Fresnel factor for all dielectrics.
const vec3 Fdielectric = vec3(0.04);

struct VertexOutput
{
	vec3 WorldPosition;
	vec3 Normal;
	vec2 TexCoord;
	mat3 WorldNormals;
	mat3 WorldTransform;
	vec3 Binormal;

	vec3 ShadowMapCoords[4];
	vec3 ViewPosition;
};

layout(location = 0) in VertexOutput Input;

layout(location = 0) out vec4 o_Color;
layout(location = 1) out vec4 o_ViewNormalsLuminance;
layout(location = 2) out vec4 o_MetalnessRoughness;
layout(location = 3) out vec4 o_AlbedoColor;

layout(push_constant) uniform MaterialUniform
{
	// 16-byte aligned members
	layout(offset = 16) vec4 AlbedoColor;

	// 12-byte aligned float3, followed by 4-byte members to avoid padding
	vec3 SpecularColor; // 12 bytes, aligned to 16 bytes
	float Specular;     // 4 bytes (fills the padding after SpecularColor)

	vec3 AttenuationColor;     // 12 bytes, aligned to 16 bytes
	float AttenuationDistance; // 4 bytes (fills the padding after AttenuationColor)

	vec3 SheenColor;      // 12 bytes, aligned to 16 bytes
	float SheenRoughness; // 4 bytes (fills the padding after SheenColor)

	// 8-byte aligned members
	float Roughness;   // 8 bytes, aligned to 8 bytes
	float Metalness;   // 4 bytes, aligned to 4 bytes
	float Emission;    // 4 bytes, aligned to 4 bytes
	bool UseNormalMap; // 4 bytes, aligned to 4 bytes

	uint AlbedoTexIndex;    // 4 bytes, aligned to 4 bytes
	uint NormalTexIndex;    // 4 bytes, aligned to 4 bytes
	uint RoughnessTexIndex; // 4 bytes, aligned to 4 bytes
	uint ClearcoatTexIndex; // 4 bytes, aligned to 4 bytes

	uint TransmissionTexIndex; // 4 bytes, aligned to 4 bytes
	uint MetalnessTexIndex;    // 4 bytes, aligned to 4 bytes
	uint EmissionTexIndex;     // 4 bytes, aligned to 4 bytes
	float IOR;                 // 4 bytes, aligned to 4 bytes

	float Transmission;       // 4 bytes, aligned to 4 bytes
	float Thickness;          // 4 bytes, aligned to 4 bytes
	float Clearcoat;          // 4 bytes, aligned to 4 bytes
	float ClearcoatRoughness; // 4 bytes, aligned to 4 bytes

	float Iridescence;          // 4 bytes, aligned to 4 bytes
	float IridescenceIor;       // 4 bytes, aligned to 4 bytes
	float IridescenceThickness; // 4 bytes, aligned to 4 bytes
	uint Flags;                 // 4 bytes, aligned to 4 bytes
}
u_MaterialUniforms;

vec3 IBL(vec3 F0, vec3 Lr)
{
	vec3 irradiance = texture(u_EnvIrradianceTex, m_Params.Normal, u_SceneData.MipBias).rgb;
	vec3 F = FresnelSchlickRoughness(F0, m_Params.NdotV, m_Params.Roughness);
	vec3 kd = (1.0 - F) * (1.0 - m_Params.Metalness);
	vec3 diffuseIBL = m_Params.Albedo.rgb * irradiance;

	int envRadianceTexLevels = textureQueryLevels(u_EnvRadianceTex);
	vec3 specularIrradiance = textureLod(u_EnvRadianceTex, RotateVectorAboutY(0.0, Lr), m_Params.Roughness * envRadianceTexLevels).rgb;

	vec2 specularBRDF = texture(u_BRDFLUTTexture, vec2(m_Params.NdotV, m_Params.Roughness), u_SceneData.MipBias).rg;
	vec3 specularIBL = specularIrradiance * (F0 * specularBRDF.x + specularBRDF.y);

	return kd * diffuseIBL + specularIBL;
}

/////////////////////////////////////////////

vec3 GetGradient(float value)
{
	vec3 zero = vec3(0.0, 0.0, 0.0);
	vec3 white = vec3(0.0, 0.1, 0.9);
	vec3 red = vec3(0.2, 0.9, 0.4);
	vec3 blue = vec3(0.8, 0.8, 0.3);
	vec3 green = vec3(0.9, 0.2, 0.3);

	float step0 = 0.0f;
	float step1 = 2.0f;
	float step2 = 4.0f;
	float step3 = 8.0f;
	float step4 = 16.0f;

	vec3 color = mix(zero, white, smoothstep(step0, step1, value));
	color = mix(color, white, smoothstep(step1, step2, value));
	color = mix(color, red, smoothstep(step1, step2, value));
	color = mix(color, blue, smoothstep(step2, step3, value));
	color = mix(color, green, smoothstep(step3, step4, value));

	return color;
}

void main()
{
	// Standard PBR inputs
	vec4 albedoTexColor = texture(GetTex(u_MaterialUniforms.AlbedoTexIndex), Input.TexCoord, u_SceneData.MipBias);
	m_Params.Albedo = albedoTexColor * ToLinear(u_MaterialUniforms.AlbedoColor); // MaterialUniforms.AlbedoColor is perceptual, must be converted to linear.
	float alpha = albedoTexColor.a;
	// note: Metalness and roughness could be in the same texture.
	//       Per GLTF spec, we read metalness from the B channel and roughness from the G channel
	//       This will still work if metalness and roughness are independent greyscale textures,
	//       but it will not work if metalness and roughness are independent textures containing only R channel.
	m_Params.Metalness = texture(GetTex(u_MaterialUniforms.MetalnessTexIndex), Input.TexCoord, u_SceneData.MipBias).b * u_MaterialUniforms.Metalness;
	m_Params.Roughness = texture(GetTex(u_MaterialUniforms.RoughnessTexIndex), Input.TexCoord, u_SceneData.MipBias).g * u_MaterialUniforms.Roughness;
	o_MetalnessRoughness = vec4(m_Params.Metalness, m_Params.Roughness, 0.f, 1.f);
	o_AlbedoColor = vec4(m_Params.Albedo);
	m_Params.Roughness = max(m_Params.Roughness, 0.05); // Minimum roughness of 0.05 to keep specular highlight

	// Normals (either from vertex or map)
	m_Params.Normal = normalize(Input.Normal);
	if (u_MaterialUniforms.UseNormalMap)
	{
		m_Params.Normal = normalize(UnpackNormalMap(texture(GetTex(u_MaterialUniforms.NormalTexIndex), Input.TexCoord, u_SceneData.MipBias)).xyz);
		m_Params.Normal = normalize(Input.WorldNormals * m_Params.Normal);
	}
	// View normals
	o_ViewNormalsLuminance.xyz = ((mat3(u_Camera.ViewMatrix) * m_Params.Normal) + 1.0) * 0.5;

	m_Params.View = normalize(u_SceneData.CameraPosition - Input.WorldPosition);
	m_Params.NdotV = max(dot(m_Params.Normal, m_Params.View), 0.0);

	// Specular reflection vector
	vec3 Lr = 2.0 * m_Params.NdotV * m_Params.Normal - m_Params.View;

	// Fresnel reflectance, metals use albedo
	vec3 F0 = mix(Fdielectric, m_Params.Albedo.rgb, m_Params.Metalness);
	float shadowScale = 1.0;

	uint cascadeIndex = 0;
	if (u_SceneData.DirectionalLights.CastsShadows)
	{
		const uint SHADOW_MAP_CASCADE_COUNT = 4;
		for (uint i = 0; i < SHADOW_MAP_CASCADE_COUNT - 1; i++)
		{
			if (Input.ViewPosition.z < u_RendererData.CascadeSplits[i]) cascadeIndex = i + 1;
		}

		float shadowDistance = u_RendererData.MaxShadowDistance; // u_CascadeSplits[3];
		float transitionDistance = u_RendererData.ShadowFade;
		float distance = length(Input.ViewPosition);
		ShadowFade = distance - (shadowDistance - transitionDistance);
		ShadowFade /= transitionDistance;
		ShadowFade = clamp(1.0 - ShadowFade, 0.0, 1.0);

		bool fadeCascades = u_RendererData.CascadeFading;
		if (fadeCascades)
		{
			float cascadeTransitionFade = u_RendererData.CascadeTransitionFade;

			float c0 = smoothstep(u_RendererData.CascadeSplits[0] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[0] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
			float c1 = smoothstep(u_RendererData.CascadeSplits[1] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[1] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
			float c2 = smoothstep(u_RendererData.CascadeSplits[2] + cascadeTransitionFade * 0.5f, u_RendererData.CascadeSplits[2] - cascadeTransitionFade * 0.5f, Input.ViewPosition.z);
			if (c0 > 0.0 && c0 < 1.0)
			{
				// Sample 0 & 1
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 0);
				float shadowAmount0 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 0, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 0, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
				float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords);

				shadowScale = mix(shadowAmount0, shadowAmount1, c0);
			}
			else if (c1 > 0.0 && c1 < 1.0)
			{
				// Sample 1 & 2
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 1);
				float shadowAmount1 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 1, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
				float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords);

				shadowScale = mix(shadowAmount1, shadowAmount2, c1);
			}
			else if (c2 > 0.0 && c2 < 1.0)
			{
				// Sample 2 & 3
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 2);
				float shadowAmount2 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 2, shadowMapCoords);
				shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, 3);
				float shadowAmount3 = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, 3, shadowMapCoords, u_RendererData.LightSize)
				                                                 : HardShadows_DirectionalLight(u_ShadowMapTexture, 3, shadowMapCoords);

				shadowScale = mix(shadowAmount2, shadowAmount3, c2);
			}
			else
			{
				vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
				shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, u_RendererData.LightSize)
				                                         : HardShadows_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords);
			}
		}
		else
		{
			vec3 shadowMapCoords = GetShadowMapCoords(Input.ShadowMapCoords, cascadeIndex);
			shadowScale = u_RendererData.SoftShadows ? PCSS_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords, u_RendererData.LightSize)
			                                         : HardShadows_DirectionalLight(u_ShadowMapTexture, cascadeIndex, shadowMapCoords);
		}

		shadowScale = 1.0 - clamp(u_SceneData.DirectionalLights.ShadowAmount - shadowScale, 0.0f, 1.0f);
	}

	// Direct lighting
	vec3 lightContribution = CalculateDirLights(F0) * shadowScale;
	lightContribution += CalculatePointLights(F0, Input.WorldPosition, ivec2(gl_FragCoord));
	lightContribution += CalculateSpotLights(F0, Input.WorldPosition, ivec2(gl_FragCoord)) * SpotShadowCalculation(u_SpotShadowTexture, Input.WorldPosition, ivec2(gl_FragCoord));
	lightContribution += m_Params.Albedo.rgb * u_MaterialUniforms.Emission;

	// Indirect lighting
	// vec3 iblContribution = IBL(F0, Lr);
	vec3 iblContribution = IBL(F0, Lr) * u_SceneData.EnvironmentMapIntensity;

	// Final color
	o_Color = vec4(iblContribution + lightContribution, 1.0);

	// TODO: Temporary bug fix.
	if (u_SceneData.DirectionalLights.Intensity <= 0.0f) shadowScale = 0.0f;

	// Shadow mask with respect to bright surfaces.
	o_ViewNormalsLuminance.a = clamp(shadowScale + dot(o_Color.rgb, vec3(0.2125f, 0.7154f, 0.0721f)), 0.0f, 1.0f);

	if (u_RendererData.ShowLightComplexity)
	{
		int pointLightCount = GetPointLightCount(ivec2(gl_FragCoord));
		int spotLightCount = GetSpotLightCount(ivec2(gl_FragCoord));

		float value = float(pointLightCount + spotLightCount);
		o_Color.rgb = (o_Color.rgb * 0.2) + GetGradient(value);
	}

	// TODO: Have a separate render pass for translucent and transparent objects.
	// Because we use the pre-depth image for depth test.
	// o_Color.a = alpha;

	// (shading-only)
	// o_Color.rgb = vec3(1.0) * shadowScale + 0.2f;

	if (u_RendererData.ShowCascades)
	{
		switch (cascadeIndex)
		{
			case 0:
				o_Color.rgb *= vec3(1.0f, 0.25f, 0.25f);
				break;
			case 1:
				o_Color.rgb *= vec3(0.25f, 1.0f, 0.25f);
				break;
			case 2:
				o_Color.rgb *= vec3(0.25f, 0.25f, 1.0f);
				break;
			case 3:
				o_Color.rgb *= vec3(1.0f, 1.0f, 0.25f);
				break;
		}
	}
}
