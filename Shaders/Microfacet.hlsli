#ifndef __MICROFACET_HLSLI__
#define __MICROFACET_HLSLI__
#include "Constants.hlsli"
#include "Reflection.hlsli"

/* 
    A microfacet surface is represented by the distribution function D, which gives the differential area of microfacets with
    the surface normal wh

    Any function that prefixes with D_ is a distribution function that gives the differential area of microfacets with the surface normal wh.
    the suffix gives the name of a specific microfacet model implementation

    The distribution function we're implementing here is an anisotropic distribution, where the normal distribution
    also varies depending on the azimuthal orientation of wh. For example, given a ax for microfacets oriented perpendicular
    to the x axis and ay for the y axis, then a value for intermediate orientations can be interpolated by constructing an ellipse 
    through these values.
*/
struct BeckmannDistribution
{
    float alphax;
    float alphay;
};

struct TrowbridgeReitzDistribution
{
    float alphax;
    float alphay;
};

float EvaluateD(in float3 wh, in BeckmannDistribution beckmann)
{
    float tan2Theta = Tan2Theta(wh);
    if (isfinite(tan2Theta))
        return 0.0f;
    float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    
    float numerator = exp(-tan2Theta * (Cos2Phi(wh) / (beckmann.alphax * beckmann.alphax) + 
                                        Sin2Phi(wh) / (beckmann.alphay * beckmann.alphay)));
    float denominator = s_PI * beckmann.alphax * beckmann.alphay * cos4Theta;
    return numerator / denominator;
}
float EvaluateLambda(in float3 w, in BeckmannDistribution beckmann)
{
    float absTanTheta = abs(TanTheta(w));
    if (isfinite(absTanTheta))
        return 0.0f;

    // Calculae alpha for direction w
    float alpha = sqrt(Cos2Phi(w) * (beckmann.alphax * beckmann.alphax) +
                       Sin2Phi(w) * (beckmann.alphay * beckmann.alphay));

    float a = 1.0f / (alpha * absTanTheta);
    if (a >= 1.6f)
        return 0.0f;

    float numerator = 1.0f - 1.259f * a + 0.396f * a * a;
    float denominator = 3.535f * a + 2.181f * a * a;
    return numerator / denominator;
}
float EvaluateG1(in float3 w, in BeckmannDistribution beckmann)
{
    return 1.0f / (1.0f + EvaluateLambda(w, beckmann));
}
float EvaluateG(in float3 wo, in float3 wi, in BeckmannDistribution beckmann)
{
    return 1.0f / (1.0f + EvaluateG1(wo, beckmann) + EvaluateG1(wi, beckmann));
}

float EvaluateD(in float3 wh, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    float tan2Theta = Tan2Theta(wh);
    if (isfinite(tan2Theta))
        return 0.0f;

    float cos4Theta = Cos2Theta(wh) * Cos2Theta(wh);
    float e = (Cos2Phi(wh) / (trowbridgeReitz.alphax * trowbridgeReitz.alphax) +
               Sin2Phi(wh) / (trowbridgeReitz.alphay * trowbridgeReitz.alphay)) * tan2Theta;

    float denominator = s_PI * trowbridgeReitz.alphax * trowbridgeReitz.alphax * cos4Theta * (1.0f + e) * (1.0f + e);
    return 1.0f / denominator;
}
float EvaluateLambda(in float3 w, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    float absTanTheta = abs(TanTheta(w));
    if (isfinite(absTanTheta))
        return 0.0f;

    // Calculae alpha for direction w
    float alpha = sqrt(Cos2Phi(w) * (trowbridgeReitz.alphax * trowbridgeReitz.alphax) +
                       Sin2Phi(w) * (trowbridgeReitz.alphay * trowbridgeReitz.alphay));

    float alpha2Tan2Theta = (alpha * absTanTheta) * (alpha * absTanTheta);

    float numerator = -1.0f + sqrt(1.0f + alpha2Tan2Theta);
    float denominator = 2.0f;
    return numerator / denominator;
}
float EvaluateG1(in float3 w, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    return 1.0f / (1.0f + EvaluateLambda(w, trowbridgeReitz));
}
float EvaluateG(in float3 wo, in float3 wi, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    return 1.0f / (1.0f + EvaluateG1(wo, trowbridgeReitz) + EvaluateG1(wi, trowbridgeReitz));
}
#endif