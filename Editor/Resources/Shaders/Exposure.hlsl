// ----------------------------------------
// -- Beyond Engine Motion Vectors shader --
// ----------------------------------------
// - Mostly adopted from XeGTAO repository.
#pragma stage : comp
#include <Buffers.hlslh>
#include <Raytracing/Descriptors.hlslh>

struct ExposurePushConstant
{
	float Exposure;
};

VK_PUSH_CONST ConstantBuffer<ExposurePushConstant> pushConst;

[[vk::binding(0, 1)]] Texture2D u_ColorMap : register(t0);
[[vk::binding(1, 1)]] RWTexture2D<half> o_Exposure : register(u0);

// Compute shader
[numthreads(16, 16, 1)] // Adjust the thread group size according to your needs
        void
        main(uint3 DTid
             : SV_DispatchThreadID)
{
	// Initialize the output motion vector
	float4 OutExposure = float4(0, 0, 0, 1);

	float2 Velocity = (u_ColorMap[DTid.xy].xy);

	// Write the result to the output motion vector texture
	o_Exposure[DTid.xy] = pushConst.Exposure;
}
