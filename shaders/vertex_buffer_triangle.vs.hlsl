struct Input
{
	float3 Position : TEXCOORD0; // Why is this TEXCOORD0? Shouldn't it be POSITION?
	float4 Color : TEXCOORD1;
};

struct Output
{
	float4 Color : TEXCOORD0;
	float4 Position : SV_Position;
};

Output main(Input input)
{
	Output output;
	output.Color = input.Color;
	output.Position = float4(input.Position, 1.0f);
	return output;
}
