struct Push
{
	[[vk::offset(4)]] uint materialIndex;
};

struct Material
{
	// Material properties
	float4 albedoFactor;
	float3 emissiveFactor;
	float emissiveStrength;
	float occlusionStrength;
	float metallicFactor;
	float roughnessFactor;
	float _padding;

	// Bindless indices
	uint albedoIndex;
	uint normalIndex;
	uint emissiveIndex;
	uint ormIndex;
};

struct VSOutput
{
	float4 pos : SV_Position;
	float3 worldPos : POSITION0;
	float4 color : COLOR0;
	float2 uv : TEXCOORD0;
};

[[vk::push_constant]] Push pc;
[[vk::binding(0, 0)]] StructuredBuffer<Material> mats;
[[vk::binding(1, 0)]] SamplerState samp;
[[vk::binding(2, 0)]] Texture2D textures[];

float4 main(VSOutput input) : SV_Target
{
	Material mat = mats[pc.materialIndex];

	uint albedoIndex   = mat.albedoIndex;
	float4 albedoColor = textures[albedoIndex].Sample(samp, input.uv);
	albedoColor        = albedoColor * mat.albedoFactor;

	uint normalIndex   = mat.normalIndex;
	float3 normalValue = textures[normalIndex].Sample(samp, input.uv).rgb;

	float4 ormValues = textures[mat.ormIndex].Sample(samp, input.uv);
	float ao         = ormValues.r;
	float roughness  = ormValues.g;
	float metallic   = ormValues.b;

	float3 emissive = textures[mat.emissiveIndex].Sample(samp, input.uv).rgb;
	emissive        = emissive * mat.emissiveFactor;

	return float4(albedoColor.rgb + emissive, albedoColor.a);
}
