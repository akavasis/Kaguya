struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};

/*
	In reality, this shader just produces a big triangle that fits the entire NDC screen
*/
VSInput main(uint VertexID : SV_VertexID)
{
	VSInput output;
	output.Texture = float2((VertexID << 1) & 2, VertexID & 2);
	output.Position = float4(output.Texture.x * 2.0f - 1.0f, -output.Texture.y * 2.0f + 1.0f, 0.0f, 1.0f);
	return output;
}