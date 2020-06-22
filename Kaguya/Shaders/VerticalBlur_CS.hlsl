// Reference to 3D Game Programming With DirectX 12 by Frank D. Luna
static const int MaxBlurRadius = 5;

cbuffer Kernel : register(b0)
{
	// We cannot have an array entry in a constant buffer that gets mapped onto
	// root constants, so list each element.  
	int BlurRadius; // This should be 5 to index into weights for multiplying blur weights
	// Support up to 11 blur weights.
	float w0;
	float w1;
	float w2;
	float w3;
	float w4;
	float w5;
	float w6;
	float w7;
	float w8;
	float w9;
	float w10;
};

Texture2D InputTexture2D : register(t0);
RWTexture2D<float4> OutputRWTexture2D : register(u0);

#define N 256
#define CacheSize (N + 2 * MaxBlurRadius)
groupshared float4 LocalThreadStorage[CacheSize];

[numthreads(1, N, 1)]
void main(int3 GTid : SV_GroupThreadID, int3 DTid : SV_DispatchThreadID)
{
	float weights[11] = { w0, w1, w2, w3, w4, w5, w6, w7, w8, w9, w10 };
	uint2 inputDimension;
	InputTexture2D.GetDimensions(inputDimension.x, inputDimension.y);
	
	// Fill local thread storage to reduce bandwidth.  To blur 
	// N pixels, we will need to load N + 2*BlurRadius pixels
	// due to the blur radius.
	
	// This thread group runs N threads.  To get the extra 2*BlurRadius pixels, 
	// have 2*BlurRadius threads sample an extra pixel.
	if (GTid.y < BlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = max(DTid.y - BlurRadius, 0);
		LocalThreadStorage[GTid.y] = InputTexture2D[int2(DTid.x, y)];
	}
	if (GTid.y >= N - BlurRadius)
	{
		// Clamp out of bound samples that occur at image borders.
		int y = min(DTid.y + BlurRadius, inputDimension.y - 1);
		LocalThreadStorage[GTid.y + 2 * BlurRadius] = InputTexture2D[int2(DTid.x, y)];
	}
	
	// Clamp out of bound samples that occur at image borders.
	LocalThreadStorage[GTid.y + BlurRadius] = InputTexture2D[min(DTid.xy, inputDimension - 1)];

	// Wait for all threads to finish in the group
	GroupMemoryBarrierWithGroupSync();
	
	// Now blur each pixel.
	float4 blurColor = float4(0, 0, 0, 0);
	
	for (int i = -BlurRadius; i <= BlurRadius; ++i)
	{
		int k = GTid.y + BlurRadius + i;
		blurColor += weights[i + BlurRadius] * LocalThreadStorage[k];
	}
	
	OutputRWTexture2D[DTid.xy] = blurColor;
}