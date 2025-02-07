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

Output main(Input input)
{
	Output output;
	output.TexCoord = input.TexCoord;

	float3 pos = (input.Position * 0.25f) - float3(0.75f, 0.75f, 0.0f);
	pos.x += float(input.instance_id % 4) * 0.5f;
	pos.y += floor(float(input.instance_id / 4)) * 0.5f;

	output.Position = float4(pos, 1.0f);
	return output;
}
