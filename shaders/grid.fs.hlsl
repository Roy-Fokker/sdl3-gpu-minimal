
struct Input
{
	float4 NearPoint : TEXCOORD0;
	float4 FarPoint : TEXCOORD1;
};

struct Output
{
	float4 color : SV_Target;
	float depth : SV_Depth;
};

struct FrameBuffer
{
	float4x4 projection;
	float4x4 view;
};

ConstantBuffer<FrameBuffer> ubo : register(b0, space3);

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

float compute_depth(float3 pos)
{
	float4 clip_space_pos = mul(ubo.projection, mul(ubo.view, float4(pos.xyz, 1.f)));
	return clip_space_pos.z / clip_space_pos.w;
}

Output main(Input input)
{
	float t = -input.NearPoint.y / (input.FarPoint.y - input.NearPoint.y);

	float3 pos = input.NearPoint.xyz + t * (input.FarPoint.xyz - input.NearPoint.xyz);

	float4 outColor = grid(pos, 10) * float(t > 0);
	float depth = compute_depth(pos);

	Output output;
	output.color = outColor;
	output.depth = depth;

	return output;
}