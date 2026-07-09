struct VertexOutput
{
	float4 pos : SV_POSITION;
	float3 col : COLOR;
};

float4 main(VertexOutput vo) : SV_TARGET
{
	return float4(vo.col, 1.0f);
};
