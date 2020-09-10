#ifndef __MICROFACET_HLSLI__
#define __MICROFACET_HLSLI__
#include "Constants.hlsli"
#include "Reflection.hlsli"

/* 
    A microfacet surface is represented by the distribution function D, which gives the differential area of microfacets with
    the surface normal wh

    The D method is a distribution function that gives the differential area of microfacets with the surface normal wh.

    The distribution function we're implementing here is an anisotropic distribution, where the normal distribution
    also varies depending on the azimuthal orientation of wh. For example, given a ax for microfacets oriented perpendicular
    to the x axis and ay for the y axis, then a value for intermediate orientations can be interpolated by constructing an ellipse 
    through these values.
*/

float RoughnessToAlphaBeckmannDistribution(float roughness)
{
	roughness = max(roughness, 1e-3);
	float x = log(roughness);
	return 1.62142f + 0.819955f * x + 0.1734f * x * x +
               0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

struct BeckmannDistribution
{
    float alphax;
    float alphay;
    
    float D(in float3 wh)
	{
		float tan2Theta = Tan2Theta(wh);
		if (isfinite(tan2Theta))
			return 0.0f;
		float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    
		float numerator = exp(-tan2Theta * (Cos2Phi(wh) / (alphax * alphax) +
                                        Sin2Phi(wh) / (alphay * alphay)));
		float denominator = s_PI * alphax * alphay * cos4Theta;
		return numerator / denominator;
	}
    
	float Lambda(in float3 w)
	{
		float absTanTheta = abs(TanTheta(w));
		if (isfinite(absTanTheta))
			return 0.0f;

        // Calculae alpha for direction w
		float alpha = sqrt(Cos2Phi(w) * (alphax * alphax) +
                       Sin2Phi(w) * (alphay * alphay));

		float a = 1.0f / (alpha * absTanTheta);
		if (a >= 1.6f)
			return 0.0f;

		float numerator = 1.0f - 1.259f * a + 0.396f * a * a;
		float denominator = 3.535f * a + 2.181f * a * a;
		return numerator / denominator;
	}
    
    float G1(in float3 w)
	{
		return 1.0f / (1.0f + Lambda(w));
	}
    
	float G(in float3 wo, in float3 wi)
	{
		return 1.0f / (1.0f + Lambda(wo) + Lambda(wi));
	}
};

BeckmannDistribution InitBeckmannDistribution(float alphax, float alphay)
{
	BeckmannDistribution distribution = { alphax, alphay };
	return distribution;
}

float RoughnessToAlphaTrowbridgeReitzDistribution(float roughness)
{
	roughness = max(roughness, 1e-3);
	float x = log(roughness);
	return 1.62142f + 0.819955f * x + 0.1734f * x * x +
               0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

struct TrowbridgeReitzDistribution
{
    float alphax;
    float alphay;
    
	float D(in float3 wh)
	{
		float tan2Theta = Tan2Theta(wh);
		if (isfinite(tan2Theta))
			return 0.0f;

		float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
		float e = (Cos2Phi(wh) / (alphax * alphax) +
               Sin2Phi(wh) / (alphay * alphay)) * tan2Theta;

		float denominator = s_PI * alphax * alphax * cos4Theta * (1.0f + e) * (1.0f + e);
		return 1.0f / denominator;
	}
    
	float Lambda(in float3 w)
	{
		float absTanTheta = abs(TanTheta(w));
		if (isfinite(absTanTheta))
			return 0.0f;

		// Calculae alpha for direction w
		float alpha = sqrt(Cos2Phi(w) * (alphax * alphax) +
                       Sin2Phi(w) * (alphay * alphay));

		float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);

		float numerator = -1.0f + sqrt(1.0f + alpha2Tan2Theta);
		float denominator = 2.0f;
		return numerator / denominator;
	}
    
	float G1(in float3 w)
	{
		return 1.0f / (1.0f + Lambda(w));
	}
    
	float G(in float3 wo, in float3 wi)
	{
		return 1.0f / (1.0f + Lambda(wo) + Lambda(wi));
	}
};

TrowbridgeReitzDistribution InitTrowbridgeReitzDistribution(float alphax, float alphay)
{
	TrowbridgeReitzDistribution distribution = { alphax, alphay };
	return distribution;
}
#endif