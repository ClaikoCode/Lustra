struct VSOutput
{
	float4 pos : SV_Position;
	float3 worldPos : POSITION0;
	float4 color : COLOR0;
};

float4 main(VSOutput input) : SV_Target
{
	// face normal from the derivatives of view-space position across the triangle.
	// ddx/ddy give the two in-plane edge vectors; their cross is the face normal.
	float3 dpdx   = ddx(input.worldPos);
	float3 dpdy   = ddy(input.worldPos);
	float3 normal = normalize(cross(dpdx, dpdy));
	return float4(normal * 0.5 + 0.5, 1.0); // visualize the normal as RGB

	// return float4(input.worldPos, 1.0f);
}
