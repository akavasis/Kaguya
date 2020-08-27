#ifndef __BXDF_HLSLI__
#define __BXDF_HLSLI__
#include "Microfacet.hlsli"

struct FresnelDielectric
{
    float cosThetaI;
    float etaI;
    float etaT;
};

struct FresnelConductor
{
    float cosThetaI;
    float etaI;
    float etaT;
    float k;    // Represents absorbton coefficient
};

float3 EvaluateFresnel(in FresnelDielectric dielectric)
{
    dielectric.cosThetaI = clamp(dielectric.cosThetaI, -1.0f, 1.0f);
    bool entering = dielectric.cosThetaI > 0.0f;
    if (!entering)
    {
        float temp = dielectric.etaI;
        dielectric.etaI = dielectric.etaT;
        dielectric.etaT = temp;
        dielectric.cosThetaI = abs(dielectric.cosThetaI);
    }

    // Compute cosThetaT using Snell's law
    float sinThetaI = sqrt(max(0.0f, 1.0f - dielectric.cosThetaI * dielectric.cosThetaI));
    float sinThetaT = dielectric.etaI / dielectric.etaT * sinThetaI;

    // Handle total internal reflection
    if (sinThetaT >= 1.0f)
        return 1.0f;
    
    float cosThetaT = sqrt(max(0.0f, 1.0f - sinThetaT * sinThetaT));
    float Rparl = ((dielectric.etaT * dielectric.cosThetaI) - (dielectric.etaI * cosThetaT)) /
                  ((dielectric.etaT * dielectric.cosThetaI) + (dielectric.etaI * cosThetaT));
    float Rperp = ((dielectric.etaI * dielectric.cosThetaI) - (dielectric.etaT * cosThetaT)) /
                  ((dielectric.etaI * dielectric.cosThetaI) + (dielectric.etaT * cosThetaT));
    return (Rparl * Rparl + Rperp * Rperp) / 2.0f;
}

float3 EvaluateFresnel(in FresnelConductor conductor)
{
    conductor.cosThetaI = clamp(conductor.cosThetaI, -1.0f, 1.0f);
    float3 eta = conductor.etaT / conductor.etaI;
    float3 etak = conductor.k / conductor.etaI;

    float cos2ThetaI = conductor.cosThetaI * conductor.cosThetaI;
    float sin2ThetaI = 1.0f - cos2ThetaI;
    float3 eta2 = eta * eta;
    float3 etak2 = etak * etak;

    float3 t0 = eta2 - etak - sin2ThetaI;
    float3 a2plusb2 = sqrt(t0 * t0 + 4.0f * eta2 * etak2);
    float3 t1 = a2plusb2 + cos2ThetaI;
    float3 a = sqrt(0.5f * (a2plusb2 + t0));
    float3 t2 = 2.0f * conductor.cosThetaI * a;
    float3 Rs = (t1 - t2) / (t1 + t2);

    float3 t3 = cos2ThetaI * a2plusb2 + sin2ThetaI * sin2ThetaI;
    float3 t4 = t2 * sin2ThetaI;
    float3 Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5f * (Rp + Rs);
}

float3 Fresnel_Schlick(in float3 Rs, in float cosTheta)
{
    return Rs + pow(1.0f - cosTheta, 5.0f) * (1.0f - Rs);
}

// Returns the value of the distribution function for the given pair of directions
float3 BRDF_TorranceSparrow(in float3 wo, in float3 wi, in float3 R, in FresnelDielectric dielectric, in BeckmannDistribution beckmann)
{
    float cosThetaO = AbsCosTheta(wo);
    float cosThetaI = AbsCosTheta(wi);
    float3 wh = wo + wi;
    if (cosThetaO == 0.0f || cosThetaI == 0.0f)
        return 0.0f;
    if (wh == (float3)0.0f)
        return 0.0f;
    wh = normalize(wh);

    float3 F = EvaluateFresnel(dielectric);
    return R * EvaluateD(wh, beckmann) * EvaluateG(wo, wi, beckmann) * F / (4.0f * cosThetaI * cosThetaO);
}
float3 BRDF_TorranceSparrow(in float3 wo, in float3 wi, in float3 R, in FresnelDielectric dielectric, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    float cosThetaO = AbsCosTheta(wo);
    float cosThetaI = AbsCosTheta(wi);
    float3 wh = wo + wi;
    if (cosThetaO == 0.0f || cosThetaI == 0.0f)
        return 0.0f;
    if (wh == (float3) 0.0f)
        return 0.0f;
    wh = normalize(wh);

    float3 F = EvaluateFresnel(dielectric);
    return R * EvaluateD(wh, trowbridgeReitz) * EvaluateG(wo, wi, trowbridgeReitz) * F / (4.0f * cosThetaI * cosThetaO);
}
float3 BRDF_AshikhminShirley(in float3 wo, in float3 wi, in float3 Rd, in float3 Rs, in BeckmannDistribution beckmann)
{
    float3 wh = wo + wi;
    if (wh == (float3) 0.0f)
        return 0.0f;
    wh = normalize(wh);
    
    float3 diffuse = (28.0f / (23.0f * s_PI)) * Rd *
        ((float3) 1.0f - Rs) *
        (1.0f - pow(1.0f - 0.5f * AbsCosTheta(wi), 5.0f)) *
        (1.0f - pow(1.0f - 0.5f * AbsCosTheta(wo), 5.0f));
    float3 specular = EvaluateD(wh, beckmann) /
                (4.0f * abs(dot(wi, wh))) *
                max(AbsCosTheta(wi), AbsCosTheta(wo)) *
                Fresnel_Schlick(Rs, dot(wi, wh));
    return diffuse + specular;
}
float3 BRDF_AshikhminShirley(in float3 wo, in float3 wi, in float3 Rd, in float3 Rs, in TrowbridgeReitzDistribution trowbridgeReitz)
{
    float3 wh = wo + wi;
    if (wh == (float3) 0.0f)
        return 0.0f;
    wh = normalize(wh);
    
    float3 diffuse = (28.0f / (23.0f * s_PI)) * Rd *
        ((float3) 1.0f - Rs) *
        (1.0f - pow(1.0f - 0.5f * AbsCosTheta(wi), 5.0f)) *
        (1.0f - pow(1.0f - 0.5f * AbsCosTheta(wo), 5.0f));
    float3 specular = EvaluateD(wh, trowbridgeReitz) /
                (4.0f * abs(dot(wi, wh))) *
                max(AbsCosTheta(wi), AbsCosTheta(wo)) *
                Fresnel_Schlick(Rs, dot(wi, wh));
    return diffuse + specular;
}
#endif