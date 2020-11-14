#include "../HLSLCommon.hlsli"
#include "SVGF_Common.hlsli"

struct RenderPassData
{
	float2 RenderTargetDimension;

	float PhiColor;
	float PhiNormal;
	uint StepSize;

	// Input Textures
	uint DenoisedSource0;
	uint DenoisedSource1;
	uint Moments;
	uint HistoryLength;
	uint Compact;

	// Output Textures
	uint RenderTarget0;
	uint RenderTarget1;
};
#define RenderPassDataType RenderPassData
#include "../ShaderLayout.hlsli"

// ----------------------------------------------------------------------------------------------------------------------------
// Computes a 3x3 gaussian blur of the variance, centered around the current pixel
float2 computeVarianceCenter(int2 ipos, Texture2D sDirect, Texture2D sIndirect)
{
	float2 sum = float2(0.0, 0.0);
	const float kernel[2][2] =
	{
		{ 1.0 / 4.0, 1.0 / 8.0 },
		{ 1.0 / 8.0, 1.0 / 16.0 }
	};

	const int radius = 1;
	for (int yy = -radius; yy <= radius; yy++)
	{
		for (int xx = -radius; xx <= radius; xx++)
		{
			int2 p = ipos + int2(xx, yy);
			float k = kernel[abs(xx)][abs(yy)];

			sum.r += sDirect[p].a * k;
			sum.g += sIndirect[p].a * k;
		}
	}

	return sum;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D DenoisedSource0 = g_Texture2DTable[g_RenderPassData.DenoisedSource0];
	Texture2D DenoisedSource1 = g_Texture2DTable[g_RenderPassData.DenoisedSource1];
	Texture2D Moments = g_Texture2DTable[g_RenderPassData.Moments];
	Texture2D HistoryLength = g_Texture2DTable[g_RenderPassData.HistoryLength];
	Texture2D Compact = g_Texture2DTable[g_RenderPassData.Compact];
	
	RWTexture2D<float4> RenderTarget0 = g_RWTexture2DTable[g_RenderPassData.RenderTarget0];
	RWTexture2D<float4> RenderTarget1 = g_RWTexture2DTable[g_RenderPassData.RenderTarget1];
	
	const int2 screenSize = int2(g_RenderPassData.RenderTargetDimension);
	const float epsVariance = 1e-10;
	const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };
	const float4 directCenter = DenoisedSource0[DTid.xy];
	const float4 indirectCenter = DenoisedSource1[DTid.xy];
	const float lDirectCenter = luminance(directCenter.rgb);
	const float lIndirectCenter = luminance(indirectCenter.rgb);

    // Variance for direct and indirect, filtered using 3x3 gaussin blur
	const float2 var = computeVarianceCenter(DTid.xy, DenoisedSource0, DenoisedSource1);

    // Number of temporally integrated pixels
	const float historyLength = HistoryLength[DTid.xy].r;

	float3 normalCenter;
	float2 zCenter;
	fetchNormalAndLinearZ(Compact, DTid.xy, normalCenter, zCenter);

	if (zCenter.x < 0)
	{
        // Not a valid depth => must be envmap => do not filter
		RenderTarget0[DTid.xy] = directCenter;
		RenderTarget1[DTid.xy] = indirectCenter;
		return;
	}

	const float phiLDirect = g_RenderPassData.PhiColor * sqrt(max(0.0, epsVariance + var.r));
	const float phiLIndirect = g_RenderPassData.PhiColor * sqrt(max(0.0, epsVariance + var.g));
	const float phiDepth = max(zCenter.y, 1e-8) * g_RenderPassData.StepSize;

    // Explicitly store/accumulate center pixel with weight 1 to prevent issues with the edge-stopping functions
	float sumWDirect = 1.0f;
	float sumWIndirect = 1.0f;
	float4 sumDirect = directCenter;
	float4 sumIndirect = indirectCenter;
	for (int yy = -2; yy <= 2; yy++)
	{
		for (int xx = -2; xx <= 2; xx++)
		{
			const int2 p = DTid.xy + int2(xx, yy) * g_RenderPassData.StepSize;
			const bool inside = all(greaterThanEqual(p, int2(0, 0))) && all(lessThan(p, screenSize));

			const float kernel = kernelWeights[abs(xx)] * kernelWeights[abs(yy)];

            // Skip center pixel, it is already accumulated
			if (inside && (xx != 0 || yy != 0))
			{
				const float4 directP = DenoisedSource0[p];
				const float4 indirectP = DenoisedSource1[p];

				float3 normalP;
				float2 zP;
				fetchNormalAndLinearZ(Compact, p, normalP, zP);
				const float lDirectP = luminance(directP.rgb);
				const float lIndirectP = luminance(indirectP.rgb);

                // Compute the edge-stopping functions
				const float2 w = computeWeight(
                    zCenter.x, zP.x, phiDepth * length(float2(xx, yy)),
					normalCenter, normalP, g_RenderPassData.PhiNormal,
                    lDirectCenter, lDirectP, phiLDirect,
                    lIndirectCenter, lIndirectP, phiLIndirect);

				const float wDirect = w.x * kernel;
				const float wIndirect = w.y * kernel;

                // Alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula
				sumWDirect += wDirect;
				sumDirect += float4(wDirect.xxx, wDirect * wDirect) * directP;

				sumWIndirect += wIndirect;
				sumIndirect += float4(wIndirect.xxx, wIndirect * wIndirect) * indirectP;
			}
		}
	}

    // Renormalization is different for variance, check paper for the formula
	RenderTarget0[DTid.xy] = float4(sumDirect / float4(sumWDirect.xxx, sumWDirect * sumWDirect));
	RenderTarget1[DTid.xy] = float4(sumIndirect / float4(sumWIndirect.xxx, sumWIndirect * sumWIndirect));
}