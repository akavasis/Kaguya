#include "../HLSLCommon.hlsli"

cbuffer RootConstants : register(b0)
{
	float2 Axis;
	float2 InverseOutputSize;

		// Bilateral filter parameters
	float DEPTH_WEIGHT;
	float NORMAL_WEIGHT;
	float PLANE_WEIGHT;
	float ANALYTIC_WEIGHT;
	float R;

	uint Source0Index;
	uint Source1Index;
	uint AnalyticIndex;
	uint NoiseEstimateIndex;
	uint NormalIndex;
	uint DepthIndex;

	uint RenderTarget0Index;
	uint RenderTarget1Index;
};
#include "../ShaderLayout.hlsli"

struct TapKey
{
	float	csZ;
	float3	csPosition;
	float3	normal;
	float	analytic;
};

float mean(float3 v)
{
	return (v.x + v.y + v.z) * (1.0f / 3.0f);
}

TapKey getTapKey(uint2 ScreenSpaceCoord, float2 ScreenSpaceUV, Camera Camera, Texture2D Analytic, Texture2D Normal, Texture2D Depth)
{
	float d = Depth[ScreenSpaceCoord].r;
	
	TapKey key;
	if ((DEPTH_WEIGHT != 0.0) || (PLANE_WEIGHT != 0.0))
	{
		key.csZ = NDCDepthToViewDepth(d, Camera);
	}

	if (PLANE_WEIGHT != 0.0)
	{
		key.csPosition = NDCDepthToViewPosition(d, ScreenSpaceUV, Camera);
	}

	if ((NORMAL_WEIGHT != 0.0) || (PLANE_WEIGHT != 0.0))
	{
		key.normal = DecodeNormal(Normal[ScreenSpaceCoord].rgb);
	}

	if (ANALYTIC_WEIGHT != 0.0)
	{
		key.analytic = mean(Analytic[ScreenSpaceCoord].rgb);
	}

	return key;
}

float calculateBilateralWeight(TapKey center, TapKey tap)
{
	float depthWeight = 1.0;
	float normalWeight = 1.0;
	float planeWeight = 1.0;
	float analyticWeight = 1.0;

	if (DEPTH_WEIGHT != 0.0)
	{
		depthWeight = max(0.0, 1.0 - abs(tap.csZ - center.csZ) * DEPTH_WEIGHT);
	}

	if (NORMAL_WEIGHT != 0.0)
	{
		float normalCloseness = dot(tap.normal, center.normal);
		normalCloseness = normalCloseness * normalCloseness;
		normalCloseness = normalCloseness * normalCloseness;

		float normalError = (1.0 - normalCloseness);
		normalWeight = max((1.0 - normalError * NORMAL_WEIGHT), 0.00);
	}

	if (PLANE_WEIGHT != 0.0)
	{
		float lowDistanceThreshold2 = 0.001;

        // Change in position in camera space
		float3 dq = center.csPosition - tap.csPosition;

        // How far away is this point from the original sample
        // in camera space? (Max value is unbounded)
		float distance2 = dot(dq, dq);

        // How far off the expected plane (on the perpendicular) is this point?  Max value is unbounded.
		float planeError = max(abs(dot(dq, tap.normal)), abs(dot(dq, center.normal)));

		planeWeight = (distance2 < lowDistanceThreshold2) ? 1.0 :
            pow(max(0.0, 1.0 - 2.0 * PLANE_WEIGHT * planeError / sqrt(distance2)), 2.0);
	}

	if (ANALYTIC_WEIGHT != 0.0)
	{
		float aDiff = abs(tap.analytic - center.analytic) * 10.0;
		analyticWeight = max(0.0, 1.0 - (aDiff * ANALYTIC_WEIGHT));
	}

	return depthWeight * normalWeight * planeWeight * analyticWeight;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	float2 ScreenSpaceUV = (DTid.xy + 0.5f) * InverseOutputSize;
	
	Texture2D Source0 = g_Texture2DTable[Source0Index];
	Texture2D Source1 = g_Texture2DTable[Source1Index];
	Texture2D Analytic = g_Texture2DTable[AnalyticIndex];
	Texture2D NoiseEstimate = g_Texture2DTable[NoiseEstimateIndex];
	Texture2D Normal = g_Texture2DTable[NormalIndex];
	Texture2D Depth = g_Texture2DTable[DepthIndex];
	
	RWTexture2D<float4> RenderTarget0 = g_RWTexture2DTable[RenderTarget0Index];
	RWTexture2D<float4> RenderTarget1 = g_RWTexture2DTable[RenderTarget1Index];
    
    // 3* is because the estimator produces larger values than we want to use, for visualization purposes
	float gaussianRadius = saturate(NoiseEstimate[DTid.xy].r * 1.5) * R;

	float3 result0, result1;
	result0 = result1 = 0.0.xxx;

    // Detect noiseless pixels and reject them
	if (gaussianRadius > 0.5)
	{
		float3 sum0 = 0.0.xxx;
		float3 sum1 = 0.0.xxx;
		float totalWeight = 0.0f;
		TapKey key = getTapKey(DTid.xy, ScreenSpaceUV, g_SystemConstants.Camera, Analytic, Normal, Depth);

		for (int r = -R; r <= R; ++r)
		{
			int2 tapOffset = int2(Axis * r);
			int2 tapLoc = DTid.xy + tapOffset;
			float2 tapLocUV = (tapLoc + 0.5f) * InverseOutputSize;

			float x = float(r) / gaussianRadius;
			float gaussian = exp(-(x * x));
			float weight = gaussian * ((r == 0) ? 1.0 : calculateBilateralWeight(key, getTapKey(tapLoc, tapLocUV, g_SystemConstants.Camera, Analytic, Normal, Depth)));
			sum0 += Source0[tapLoc].rgb * weight;
			sum1 += Source1[tapLoc].rgb * weight;
			totalWeight += weight;
		}

        // totalWeight >= gaussian[0], so no division by zero here
		result0 = sum0 / totalWeight;
		result1 = sum1 / totalWeight;
	}
	else
	{
        // No denoising needed
		result0 = Source0[DTid.xy].rgb;
		result1 = Source1[DTid.xy].rgb;
	}
	
	RenderTarget0[DTid.xy] = float4(result0, 1.0f);
	RenderTarget1[DTid.xy] = float4(result1, 1.0f);
}