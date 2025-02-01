struct Input
{
	uint VertexIndex : SV_VertexID;
};

struct Output
{
	float4 Color : TEXCOORD0;
	float4 Position : SV_Position;
};

Output main(Input input)
{
	Output vertex_list[3] = {
		{{1.0f, 0.0f, 0.0f, 1.0f}, {-1.0f, -1.0f, 0.0f, 1.0f}},
		{{0.0f, 1.0f, 0.0f, 1.0f}, { 1.0f, -1.0f, 0.0f, 1.0f}},
		{{0.0f, 0.0f, 1.0f, 1.0f}, { 0.0f,  1.0f, 0.0f, 1.0f}},
	};

	return vertex_list[input.VertexIndex];
}
