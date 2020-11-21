// Modified from the NVIDIA SVGF sample code, https://research.nvidia.com/publication/2017-07_Spatiotemporal-Variance-Guided-Filtering%3A

#include <HLSLCommon.hlsli>
#include "SVGF_Common.hlsli"

struct RenderPassData
{
	float2 RenderTargetDimension;

	float PhiColor;
	float PhiNormal;

	// Input Textures
	uint Source0;
	uint Source1;
	uint Moments;
	uint HistoryLength;
	uint Compact;

	// Output Textures
	uint RenderTarget0;
	uint RenderTarget1;
};
#define RenderPassDataType RenderPassData
#include <ShaderLayout.hlsli>

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D			Source0			= g_Texture2DTable[g_RenderPassData.Source0];
	Texture2D			Source1			= g_Texture2DTable[g_RenderPassData.Source1];
	Texture2D			Moments			= g_Texture2DTable[g_RenderPassData.Moments];
	Texture2D			HistoryLength	= g_Texture2DTable[g_RenderPassData.HistoryLength];
	Texture2D			Compact			= g_Texture2DTable[g_RenderPassData.Compact];
	
	RWTexture2D<float4> RenderTarget0	= g_RWTexture2DTable[g_RenderPassData.RenderTarget0];
	RWTexture2D<float4> RenderTarget1	= g_RWTexture2DTable[g_RenderPassData.RenderTarget1];
	
	float				historyLength	= HistoryLength[DTid.xy].r;
	int2				screenSize		= int2(g_RenderPassData.RenderTargetDimension);
	
    // Not enough temporal history available
	if (historyLength < 4.0)
	{
		float sumWDirect = 0.0;
		float sumWIndirect = 0.0;
		float3 sumDirect = float3(0.0, 0.0, 0.0);
		float3 sumIndirect = float3(0.0, 0.0, 0.0);
		float4 sumMoments = float4(0.0, 0.0, 0.0, 0.0);
		const float4 directCenter = Source0[DTid.xy];
		const float4 indirectCenter = Source1[DTid.xy];
		const float lDirectCenter = RGBToCIELuminance(directCenter.rgb);
		const float lIndirectCenter = RGBToCIELuminance(indirectCenter.rgb);

		float3 normalCenter;
		float2 zCenter;
		fetchNormalAndLinearZ(Compact, DTid.xy, normalCenter, zCenter);

        // Current pixel does not a valid depth => must be envmap => do nothing
		if (zCenter.x < 0)
		{
			RenderTarget0[DTid.xy] = directCenter;
			RenderTarget1[DTid.xy] = indirectCenter;
			return;
		}

        // Compute first and second moment spatially. This code also applies cross-bilateral filtering on the input color samples
		const float phiNormal = g_RenderPassData.PhiNormal;
		const float phiLDirect = g_RenderPassData.PhiColor;
		const float phiLIndirect = g_RenderPassData.PhiColor;
		const float phiDepth = max(zCenter.y, 1e-8) * 3.0f;
		const int radius = 3;
		for (int yy = -radius; yy <= radius; yy++)
		{
			for (int xx = -radius; xx <= radius; xx++)
			{
				const int2 p = DTid.xy + int2(xx, yy);
				const bool inside = all(greaterThanEqual(p, int2(0, 0))) && all(lessThan(p, screenSize));
				const bool samePixel = (xx == 0) && (yy == 0);
				const float kernel = 1.0f;

				if (inside)
				{
					const float3 directP = Source0[p].rgb;
					const float3 indirectP = Source1[p].rgb;
					const float4 momentsP = Moments[p];
					const float lDirectP = RGBToCIELuminance(directP.rgb);
					const float lIndirectP = RGBToCIELuminance(indirectP.rgb);

					float3 normalP;
					float2 zP;
					fetchNormalAndLinearZ(Compact, p, normalP, zP);

					const float2 w = computeWeight(
                        zCenter.x, zP.x, phiDepth * length(float2(xx, yy)),
						normalCenter, normalP, phiNormal,
                        lDirectCenter, lDirectP, phiLDirect,
                        lIndirectCenter, lIndirectP, phiLIndirect);

					const float wDirect = w.x;
					const float wIndirect = w.y;

					sumWDirect += wDirect;
					sumDirect += directP * wDirect;

					sumWIndirect += wIndirect;
					sumIndirect += indirectP * wIndirect;

					sumMoments += momentsP * float4(wDirect.xx, wIndirect.xx);
				}
			}
		}

		// Clamp sums to >0 to avoid NaNs.
		sumWDirect = max(sumWDirect, 1e-6f);
		sumWIndirect = max(sumWIndirect, 1e-6f);

		sumDirect /= sumWDirect;
		sumIndirect /= sumWIndirect;
		sumMoments /= float4(sumWDirect.xx, sumWIndirect.xx);

        // Compute variance for direct and indirect illumination using first and second moments
		float2 variance = sumMoments.ga - sumMoments.rb * sumMoments.rb;

        // Give the variance a boost for the first frames
		variance *= 4.0 / max(1.0, historyLength);

		RenderTarget0[DTid.xy] = float4(sumDirect, variance.r);
		RenderTarget1[DTid.xy] = float4(sumIndirect, variance.g);
	}
	else
	{
        // Do nothing, pass data unmodified
		RenderTarget0[DTid.xy] = Source0[DTid.xy];
		RenderTarget1[DTid.xy] = Source1[DTid.xy];
	}
}