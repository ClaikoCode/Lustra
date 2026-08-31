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

	// Bindless texture indices
	uint albedoIndex;
	uint normalIndex;
	uint emissiveIndex;
	uint ormIndex;

	// Bindless sampler indices
	uint albedoSampIndex;
	uint normalSampIndex;
	uint emissiveSampIndex;
	uint ormSampIndex;
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
[[vk::binding(1, 0)]] SamplerState samplers[];
[[vk::binding(2, 0)]] Texture2D textures[];

float4 main(VSOutput input) : SV_Target
{
	Material mat = mats[pc.materialIndex];

	SamplerState albedoSampler   = samplers[mat.albedoSampIndex];
	SamplerState normalSampler   = samplers[mat.normalSampIndex];
	SamplerState emissiveSampler = samplers[mat.emissiveSampIndex];
	SamplerState ormSampler      = samplers[mat.ormSampIndex];

	uint albedoIndex   = mat.albedoIndex;
	float4 albedoColor = textures[albedoIndex].Sample(albedoSampler, input.uv);
	albedoColor        = albedoColor * mat.albedoFactor;

	uint normalIndex   = mat.normalIndex;
	float3 normalValue = textures[normalIndex].Sample(normalSampler, input.uv).rgb;

	float3 ormValues = textures[mat.ormIndex].Sample(ormSampler, input.uv).rgb;
	float ao         = ormValues.r;
	float roughness  = ormValues.g;
	float metallic   = ormValues.b;

	float3 emissive = textures[mat.emissiveIndex].Sample(emissiveSampler, input.uv).rgb;
	emissive        = emissive * mat.emissiveFactor;

	return float4(albedoColor.rgb + emissive, albedoColor.a);
}
