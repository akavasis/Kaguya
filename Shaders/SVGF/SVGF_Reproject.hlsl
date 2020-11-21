// Modified from the NVIDIA SVGF sample code, https://research.nvidia.com/publication/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A

#include <HLSLCommon.hlsli>
#include "SVGF_Common.hlsli"

struct RenderPassData
{
	float2 RenderTargetDimension;
	
	float Alpha;
	float MomentsAlpha;
	
	// Input Textures
	uint Motion;
	uint LinearZ;
	
	uint Source0;
	uint Source1;
	
	uint PrevSource0;
	uint PrevSource1;
	uint PrevLinearZ;
	uint PrevMoments;
	uint PrevHistoryLength;
	
	// Output Textures
	uint RenderTarget0;
	uint RenderTarget1;
	uint Moments;
	uint HistoryLength;
};
#define RenderPassDataType RenderPassData
#include <ShaderLayout.hlsli>

// ----------------------------------------------------------------------------------------------------------------------------
inline bool isReprojectionValid(int2 coord, int2 imageDim, float currentZ, float previousZ, float fwidthZ, float3 currentNormal, float3 prevNormal, float fwidthNormal)
{
	bool valid = true;
	
    // Check whether reprojected pixel is inside of the screen
	if (any(lessThan(coord, int2(1, 1))) || any(greaterThan(coord, imageDim - int2(1, 1))))
	{
		valid = false;
	}

    // Check if deviation of depths is acceptable
	if (abs(previousZ - currentZ) / (fwidthZ + 1e-4) > 2.0)
	{
		valid = false;
	}

    // Check normals for compatibility
	if (distance(currentNormal, prevNormal) / (fwidthNormal + 1e-2) > 16.0)
	{
		valid = false;
	}

	return valid;
}

// ----------------------------------------------------------------------------------------------------------------------------
inline bool loadPrevData(int2 ScreenSpaceCoord, out float4 prevDirect, out float4 prevIndirect, out float4 prevMoments, out float historyLength)
{
	Texture2D Motion			= g_Texture2DTable[g_RenderPassData.Motion];
	Texture2D LinearZ			= g_Texture2DTable[g_RenderPassData.LinearZ];
	
	Texture2D PrevSource0		= g_Texture2DTable[g_RenderPassData.PrevSource0];
	Texture2D PrevSource1		= g_Texture2DTable[g_RenderPassData.PrevSource1];
	Texture2D PrevMoments		= g_Texture2DTable[g_RenderPassData.PrevMoments];
	Texture2D PrevHistoryLength = g_Texture2DTable[g_RenderPassData.PrevHistoryLength];
	Texture2D PrevLinearZ		= g_Texture2DTable[g_RenderPassData.PrevLinearZ];
	
	const float2 imageDim = g_RenderPassData.RenderTargetDimension;
	const float4 motion = Motion[ScreenSpaceCoord]; // xy = motion, z = length(fwidth(pos)), w = length(fwidth(normal))
	const int2 iposPrev = int2(float2(ScreenSpaceCoord) + motion.xy * imageDim + float2(0.5, 0.5)); // +0.5 to account for texel center offset
	const float4 depth = LinearZ[ScreenSpaceCoord]; // stores: Z, fwidth(z), z_prev
	const float3 normal = octToDir(asuint(depth.w));
	const float2 posPrev = ScreenSpaceCoord + motion.xy * imageDim;
	const int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

	prevDirect = float4(0, 0, 0, 0);
	prevIndirect = float4(0, 0, 0, 0);
	prevMoments = float4(0, 0, 0, 0);
    
    // Check for all 4 taps of the bilinear filter for validity
	bool v[4];
	bool valid = false;
	for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
	{
		int2 loc = int2(posPrev) + offset[sampleIdx];
		float4 depthPrev = PrevLinearZ[loc];
		float3 normalPrev = octToDir(asuint(depthPrev.w));

		v[sampleIdx] = isReprojectionValid(iposPrev, imageDim, depth.z, depthPrev.x, depth.y, normal, normalPrev, motion.w);

		valid = valid || v[sampleIdx];
	}

	if (valid)
	{
		float sumw = 0;
		float x = frac(posPrev.x);
		float y = frac(posPrev.y);

        // Bilinear weights
		float w[4] =
		{
			(1 - x) * (1 - y),
            x * (1 - y),
            (1 - x) * y,
            x * y
		};

		prevDirect = float4(0, 0, 0, 0);
		prevIndirect = float4(0, 0, 0, 0);
		prevMoments = float4(0, 0, 0, 0);

        // Perform the actual bilinear interpolation
		for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
		{
			int2 loc = int2(posPrev) + offset[sampleIdx];
			if (v[sampleIdx])
			{
				prevDirect += w[sampleIdx] * PrevSource0[loc];
				prevIndirect += w[sampleIdx] * PrevSource1[loc];
				prevMoments += w[sampleIdx] * PrevMoments[loc];
				sumw += w[sampleIdx];
			}
		}

		// Redistribute weights in case not all taps were used
		valid = (sumw >= 0.01);
		prevDirect = valid ? prevDirect / sumw : float4(0, 0, 0, 0);
		prevIndirect = valid ? prevIndirect / sumw : float4(0, 0, 0, 0);
		prevMoments = valid ? prevMoments / sumw : float4(0, 0, 0, 0);
	}

    // Perform cross-bilateral filter in the hope to find some suitable samples somewhere
	if (!valid)
	{
		float cnt = 0.0;

        // This code performs a binary descision for each tap of the cross-bilateral filter
		const int radius = 1;
		for (int yy = -radius; yy <= radius; yy++)
		{
			for (int xx = -radius; xx <= radius; xx++)
			{
				const int2 p = iposPrev + int2(xx, yy);
				float4 depthFilter = PrevLinearZ[p];
				float3 normalFilter = octToDir(asuint(depthFilter.w));

				if (isReprojectionValid(iposPrev, imageDim, depth.z, depthFilter.x, depth.y, normal, normalFilter, motion.w))
				{
					prevDirect += PrevSource0[p];
					prevIndirect += PrevSource1[p];
					prevMoments += PrevMoments[p];
					cnt += 1.0;
				}
			}
		}
		if (cnt > 0)
		{
			valid = true;
			prevDirect /= cnt;
			prevIndirect /= cnt;
			prevMoments /= cnt;
		}
	}

	if (valid)
	{
        // crude, fixme
		historyLength = PrevHistoryLength[iposPrev].r;
	}
	else
	{
		prevDirect = float4(0, 0, 0, 0);
		prevIndirect = float4(0, 0, 0, 0);
		prevMoments = float4(0, 0, 0, 0);
		historyLength = 0;
	}

	return valid;
}

// ----------------------------------------------------------------------------------------------------------------------------
void loadCurrData(int2 ScreenSpaceCoord, out float3 StochasticShadowed, out float3 StochasticUnshadowed)
{
	Texture2D Source0 = g_Texture2DTable[g_RenderPassData.Source0];
	Texture2D Source1 = g_Texture2DTable[g_RenderPassData.Source1];
	StochasticShadowed = Source0[ScreenSpaceCoord].rgb;
	StochasticUnshadowed = Source1[ScreenSpaceCoord].rgb;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    // Load current
	float3 stochasticShadowed, stochasticUnshadowed;
	loadCurrData(DTid.xy, stochasticShadowed, stochasticUnshadowed);

    // Load previous
	float4 prevDirect, prevIndirect, prevMoments;
	float historyLength;
	bool success;
	success = loadPrevData(DTid.xy, prevDirect, prevIndirect, prevMoments, historyLength);
	historyLength = min(32.0f, success ? historyLength + 1.0f : 1.0f);

    // This adjusts the alpha for the case where insufficient history is available.
    // It boosts the temporal accumulation to give the samples equal weights in the beginning.
	const float alpha = success ? max(g_RenderPassData.Alpha, 1.0 / historyLength) : 1.0;
	const float alphaMoments = success ? max(g_RenderPassData.MomentsAlpha, 1.0 / historyLength) : 1.0;

    // Compute first two moments of luminance
	float4 moments;
	float2 variance;
	moments.r = RGBToCIELuminance(stochasticShadowed);
	moments.b = RGBToCIELuminance(stochasticUnshadowed);
	moments.g = moments.r * moments.r;
	moments.a = moments.b * moments.b;

    // Temporal integration of the moments
	moments = lerp(prevMoments, moments, alphaMoments);
	variance = max(float2(0, 0), moments.ga - moments.rb * moments.rb);

    // Temporal integration of direct and indirect
	float4 temporalDirect = lerp(prevDirect, float4(stochasticShadowed, 0), alpha);
	float4 temporalIndirect = lerp(prevIndirect, float4(stochasticUnshadowed, 0), alpha);

    // Write out results
	RWTexture2D<float4> RenderTarget0 = g_RWTexture2DTable[g_RenderPassData.RenderTarget0];
	RWTexture2D<float4> RenderTarget1 = g_RWTexture2DTable[g_RenderPassData.RenderTarget1];
	RWTexture2D<float4> Moments = g_RWTexture2DTable[g_RenderPassData.Moments];
	RWTexture2D<float4> HistoryLength = g_RWTexture2DTable[g_RenderPassData.HistoryLength];
	
	RenderTarget0[DTid.xy] = float4(temporalDirect.rgb, variance.r);
	RenderTarget1[DTid.xy] = float4(temporalIndirect.rgb, variance.g);
	Moments[DTid.xy] = moments;
	HistoryLength[DTid.xy] = historyLength;
}