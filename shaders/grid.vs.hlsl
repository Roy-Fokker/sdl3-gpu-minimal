struct Input
{
	uint VertexIndex : SV_VertexID;
};

struct Output
{
	float4 Color : TEXCOORD0;
	float4 Position : SV_Position;
};

struct FrameBuffer
{
	float4x4 projection;
	float4x4 view;
};

ConstantBuffer<FrameBuffer> ubo : register(b0, space1);

Output main(Input input)
{
	Output grid_plane[6] = {
		// Color                    Position
		{{1.0f, 0.0f, 0.0f, 1.0f}, { -1.0f, -1.0f, 0.0f, 1.0f}},
		{{1.0f, 0.0f, 0.0f, 1.0f}, {  1.0f, -1.0f, 0.0f, 1.0f}},
		{{1.0f, 0.0f, 0.0f, 1.0f}, {  1.0f,  1.0f, 0.0f, 1.0f}},
		{{1.0f, 0.0f, 0.0f, 1.0f}, {  1.0f,  1.0f, 0.0f, 1.0f}},
		{{1.0f, 0.0f, 0.0f, 1.0f}, { -1.0f,  1.0f, 0.0f, 1.0f}},
		{{1.0f, 0.0f, 0.0f, 1.0f}, { -1.0f, -1.0f, 0.0f, 1.0f}},
	};

	Output output = grid_plane[input.VertexIndex];

	float4 pos = output.Position;
	output.Position = mul(ubo.projection, mul(ubo.view, pos));

	return output;
}
