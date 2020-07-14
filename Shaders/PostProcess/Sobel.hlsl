#pragma pack_matrix(row_major)
#include "../HLSLCommon.hlsli"

Texture2D InputTexture2D : register(t0);
RWTexture2D<float4> OutputRWTexture2D : register(u0);

[numthreads(32, 32, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float3x3 XKernel =
	{
		-1.0f, 0.0f, 1.0f,
		-2.0f, 0.0f, 2.0f,
		-1.0f, 0.0f, 1.0f
	};

	float3x3 YKernel =
	{
		-1.0f, -2.0f, -1.0f,
		0.0f, 0.0f, 0.0f,
		1.0f, 2.0f, 1.0f
	};
	
	float4 neighbors[3][3];
	for (int x = 0; x < 3; ++x)
	{
		for (int y = 0; y < 3; ++y)
		{
			int2 position = DTid.xy + int2(-1 + x, -1 + y);
			// Grayscale the input (Averaging method)
			float4 sceneColor = InputTexture2D.Load(int3(position, 0));
			// clamp to avoid artifacts from exceeding fp16 through framebuffer blending of multiple very bright lights
			sceneColor.rgb = min(float3(256 * 256, 256 * 256, 256 * 256), sceneColor.rgb);
			
			half3 linearColor = sceneColor.rgb;
			half brightness = Luminance(linearColor);
			neighbors[x][y] = float4(brightness, brightness, brightness, sceneColor.a);
		}
	}

	// Gx is equal to XKernel * A where as a is input image's neighbors
	float4 Gx =
	(XKernel[0][0] * neighbors[0][0]) + (XKernel[1][0] * neighbors[1][0]) + (XKernel[2][0] * neighbors[2][0]) +
	(XKernel[0][2] * neighbors[0][2]) + (XKernel[1][2] * neighbors[1][2]) + (XKernel[2][2] * neighbors[2][2]);
	
	// Gy is equal to YKernel * A where as a is input image's neighbors
	float4 Gy =
	(YKernel[0][0] * neighbors[0][0]) + (YKernel[0][1] * neighbors[0][1]) + (YKernel[0][2] * neighbors[0][2]) +
	(YKernel[2][0] * neighbors[2][0]) + (YKernel[2][1] * neighbors[2][1]) + (YKernel[2][2] * neighbors[2][2]);
	
	// We can use result of the gradient approximation to calculate gradient
	float4 G = sqrt((Gx * Gx) + (Gy * Gy));
	
	// Making edges black, and nonedges white.
	G = 1.0f - G;
	
	OutputRWTexture2D[DTid.xy] = G;
}