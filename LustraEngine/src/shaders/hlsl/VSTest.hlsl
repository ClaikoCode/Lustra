const static float3 positions[3] = {
    float3(0.0f, -0.5f, 0.0f), // Top
    float3(-0.5f, 0.5f, 0.0f), // Bottom left
    float3(0.5f, 0.5f, 0.0f)   // Bottom right
};

const static float3 colors[3] = {
    float3(1.0f, 0.0f, 0.0f), // Red
    float3(0.0f, 1.0f, 0.0f), // Green
    float3(0.0f, 0.0f, 1.0f)  // Blue
};

struct Input
{
	uint vertexID : SV_VertexID;
};

struct VertexOutput
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

VertexOutput main(Input input)
{
	VertexOutput vo;

	vo.pos = float4(positions[input.vertexID], 1.0f);
	vo.col = colors[input.vertexID];

	return vo;
}
