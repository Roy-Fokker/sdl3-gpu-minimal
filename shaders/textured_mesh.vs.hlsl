struct Input
{
	// Position is using TEXCOORD semantic because of rules imposed by SDL
	// Per https://wiki.libsdl.org/SDL3/SDL_CreateGPUShader#remarks
	float3 Position : TEXCOORD0; 
	float2 TexCoord : TEXCOORD1;
	uint instance_id : SV_InstanceID;
};

struct Output
{
	float2 TexCoord : TEXCOORD0;
	float4 Position : SV_Position;
};

struct FrameBuffer
{
	float4x4 mvp;
};

ConstantBuffer<FrameBuffer> ubo : register(b0, space1);

Output main(Input input)
{
	Output output;
	output.TexCoord = input.TexCoord;

	float4 pos = float4(input.Position, 1.0f);
	output.Position = mul(ubo.mvp, pos);

	return output;
}
