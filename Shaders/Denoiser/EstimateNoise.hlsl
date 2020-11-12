cbuffer RootConstants : register(b0)
{
	uint Radius;
	float2 InverseOutputSize;
	
	uint Source0Index;
	uint Source1Index;
	uint RenderTargetIndex;
};
#include "../ShaderLayout.hlsli"

//	<https://www.shadertoy.com/view/4dS3Wd>
//	By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n)
{
	return frac(sin(n) * 1e4);
}
float hash(float2 p)
{
	return frac(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
}

/* Sample the signal used for the noise estimation at offset from ssC */
float3 tap(uint2 uv, uint2 offset, Texture2D source0, Texture2D source1)
{
    // Conditional shadow; gives slightly noiser transition to penumbra but avoids noise in the umbra
	float3 n = source0[uv + offset].rgb;
	float3 d = source1[uv + offset].rgb;

	float3 result;
	for (int i = 0; i < 3; ++i)
	{
		result[i] = (d[i] < 0.000001) ? 1.0 : (n[i] / d[i]);
	}
	return result;
}

///////////////////////////////////////////////////////////////////////////////
// Estimate desired radius from the second derivative of the signal itself in source0 relative to baseline,
// which is the noisier image because it contains shadows
float estimateNoise(uint2 uv, uint2 axis, Texture2D Source0, Texture2D Source1)
{
	const int NOISE_ESTIMATION_RADIUS = min(10, Radius);
	float3 v2 = tap(uv, uint2(-NOISE_ESTIMATION_RADIUS * axis), Source0, Source1);
	float3 v1 = tap(uv, uint2((1 - NOISE_ESTIMATION_RADIUS) * axis), Source0, Source1);

	float d2mag = 0;
    // The first two points are accounted for above
	for (int r = -NOISE_ESTIMATION_RADIUS + 2; r <= NOISE_ESTIMATION_RADIUS; ++r)
	{
		float3 v0 = tap(uv, axis * r, Source0, Source1);

        // Second derivative
		float3 d2 = v2 - v1 * 2.0f + v0;
       
		d2mag += length(d2);

        // Shift weights in the window
		v2 = v1;
		v1 = v0;
	}

    // Scaled value by 1.5 *before* clamping to visualize going out of range
    // It is clamped again when applied.
	return saturate(sqrt(d2mag * (1.0 / float(Radius))) * (1.0 / 1.5));
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D Source0 = g_Texture2DTable[Source0Index];
	Texture2D Source1 = g_Texture2DTable[Source1Index];
	
	float angle = hash(DTid.x + DTid.y * 1920.0f);
	
	float result = 0.0f;
	const int N = 4;
	[unroll]
	for (float t = 0.0f; t < N; ++t, angle += s_PI / float(N))
	{
		float c = cos(angle), s = sin(angle);
		result = max(estimateNoise(DTid.xy, uint2(c, s), Source0, Source1), result);
	}
	
	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[RenderTargetIndex];
	RenderTarget[DTid.xy] = result.xxxx;
}