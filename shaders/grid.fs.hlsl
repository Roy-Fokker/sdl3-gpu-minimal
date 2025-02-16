
struct Input
{
	float4 NearPoint : TEXCOORD0;
	float4 FarPoint : TEXCOORD1;
};

float4 grid(float3 pos, float scale)
{
	float2 coord = pos.xz * scale;
	float2 derivative = fwidth(coord);
	float2 grid = abs(frac(coord - 0.5) - 0.5) / derivative;
	
	float line_ = min(grid.x, grid.y);
	float min_z = min(derivative.y, 1);
	float min_x = min(derivative.x, 1);

	float4 color = float4(0.2, 0.2, 0.2, 1.0 - min(line_, 1.0));

	if (pos.x > -0.1 * min_x && pos.x < 0.1 * min_x)
	{
		color.z = 1.0f;
	}

	if (pos.z > -0.1 * min_z && pos.z < 0.1 * min_z)
	{
		color.x = 1.0f;
	}

	return color;
}

float4 main(Input input) : SV_Target0
{
	float t = -input.NearPoint.y / (input.FarPoint.y - input.NearPoint.y);

	float3 pos = input.NearPoint.xyz + t * (input.FarPoint.xyz - input.NearPoint.xyz);

	float4 outColor = grid(pos, 10) * float(t > 0);

	return outColor;
}