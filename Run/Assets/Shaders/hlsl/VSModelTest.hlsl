struct Push
{
	uint transformIndex;
};

struct FrameConstants
{
	matrix view;
	matrix proj;
};

struct InstanceData
{
	matrix transform;
};

struct VSInput
{
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
	float3 normal : NORMAL;
	float4 tangent : TANGENT;
	float4 color : COLOR;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 worldPos : POSITION0; // pass view-space pos to PS for the normal
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

// Bindings
[[vk::push_constant]] Push pc;
[[vk::binding(0, 1)]] ConstantBuffer<FrameConstants> uFrame;      // Collapses to a uniform buffer.
[[vk::binding(1, 1)]] StructuredBuffer<InstanceData> bTransforms; // Collapses to a read-only storage buffer.

VSOutput main(VSInput input)
{
	matrix modelTransform = bTransforms[pc.transformIndex].transform;

	float4 worldPos = mul(modelTransform, float4(input.pos, 1.0));

	VSOutput output;
	output.pos      = mul(uFrame.proj, mul(uFrame.view, worldPos));
	output.worldPos = worldPos.xyz; // view-space position for the PS normal
	output.color    = input.color;
	output.uv       = input.uv;

	return output;
}
