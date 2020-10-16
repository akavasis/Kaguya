#ifndef __BXDF_HLSLI__
#define __BXDF_HLSLI__
#include "HLSLCommon.hlsli"
#include "Microfacet.hlsli"

struct FresnelDielectric
{
	float etaI;
	float etaT;
    
	float3 Evaluate(in float cosThetaI)
	{
		cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
		// Potentially swap indices of refraction
		bool entering = cosThetaI > 0.0f;
		if (!entering)
		{
			float temp = etaI;
			etaI = etaT;
			etaT = temp;
			cosThetaI = abs(cosThetaI);
		}

		// Compute cosThetaT using Snell's law
		float sinThetaI = sqrt(max(0.0f, 1.0f - cosThetaI * cosThetaI));
		float sinThetaT = etaI / etaT * sinThetaI;

		// Handle total internal reflection
		if (sinThetaT >= 1.0f)
			return 1.0f;
    
		float cosThetaT = sqrt(max(0.0f, 1.0f - sinThetaT * sinThetaT));
		float Rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) / ((etaT * cosThetaI) + (etaI * cosThetaT));
		float Rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) / ((etaI * cosThetaI) + (etaT * cosThetaT));
		return (Rparl * Rparl + Rperp * Rperp) / 2.0f;
	}
};

FresnelDielectric InitFresnelDielectric(float etaI, float etaT)
{
	FresnelDielectric fresnel = { etaI, etaT };
	return fresnel;
}

struct FresnelConductor
{
	float etaI;
	float etaT;
	float k; // Represents absorption coefficient
	
	float3 Evaluate(in float cosThetaI)
	{
		cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
		float3 eta = etaT / etaI;
		float3 etak = k / etaI;

		float cos2ThetaI = cosThetaI * cosThetaI;
		float sin2ThetaI = 1.0f - cos2ThetaI;
		float3 eta2 = eta * eta;
		float3 etak2 = etak * etak;

		float3 t0 = eta2 - etak - sin2ThetaI;
		float3 a2plusb2 = sqrt(t0 * t0 + 4.0f * eta2 * etak2);
		float3 t1 = a2plusb2 + cos2ThetaI;
		float3 a = sqrt(0.5f * (a2plusb2 + t0));
		float3 t2 = 2.0f * cosThetaI * a;
		float3 Rs = (t1 - t2) / (t1 + t2);

		float3 t3 = cos2ThetaI * a2plusb2 + sin2ThetaI * sin2ThetaI;
		float3 t4 = t2 * sin2ThetaI;
		float3 Rp = Rs * (t3 - t4) / (t3 + t4);

		return 0.5f * (Rp + Rs);
	}
};

FresnelConductor InitFresnelConductor(float etaI, float etaT, float k)
{
	FresnelConductor fresnel = { etaI, etaT, k };
	return fresnel;
}

float Fresnel_Schlick(in float IndexOfRefraction, in float cosTheta)
{
	float r0 = (1.0f - IndexOfRefraction) / (1.0f + IndexOfRefraction);
	r0 = r0 * r0;
	return r0 + (1.0f - r0) * pow((1.0f - cosTheta), 5);
}

struct MicrofacetBRDF
{
	float3 R;
	TrowbridgeReitzDistribution distribution;
	FresnelDielectric fresnel;
	
	float3 f(in float3 wo, in float3 wi)
	{
		float cosThetaO = AbsCosTheta(wo);
		float cosThetaI = AbsCosTheta(wi);
		if (cosThetaO == 0.0f || cosThetaI == 0.0f)																				
			return 0.0f;
		float3 wh = normalize(wo + wi);
																																
		return R * distribution.D(wh) * distribution.G(wo, wi) * fresnel.Evaluate(cosThetaI) / (4.0f * cosThetaI * cosThetaO);
	}
};

MicrofacetBRDF InitMicrofacetBRDF(float3 R, TrowbridgeReitzDistribution distribution, FresnelDielectric fresnel)
{
	MicrofacetBRDF brdf = { R, distribution, fresnel };
	return brdf;
}

// In PBRT, BSDF uses (s, t, n) basis to define tangent frame for the normal
// in here, we represent them using tangent, bitangent, and normal
struct BSDF
{
	float3 tangent;
	float3 bitangent;
	float3 normal;
	MicrofacetBRDF brdf;
    
	float3 WorldToLocal(in float3 v)
	{
		return float3(dot(v, tangent), dot(v, bitangent), dot(v, normal));
	}
    
	float3 LocalToWorld(in float3 v)
	{
		return float3(tangent.x * v.x + bitangent.x * v.y + normal.x * v.z,
				      tangent.y * v.x + bitangent.y * v.y + normal.y * v.z,
				      tangent.z * v.x + bitangent.z * v.y + normal.z * v.z);
	}
	
	float3 f(in float3 woW, in float3 wiW)
	{
		float3 wo = WorldToLocal(woW), wi = WorldToLocal(wiW);
		if (wo.z == 0.0f)
			return 0.0f;
		return brdf.f(wo, wi);
	}
	
	float3 Sample_f(in float3 woW, out float3 wiW, in float2 u, out float pdf)
	{
		return 0.0f;
	}
};

static const float3 Fdielectric = 0.04;

// NDF: Normal distribution function
// Approximates the amount the surface's microfacets are aligned to the halfway vector, 
// influenced by the roughness of the surface.
// this is the primary function approximating the microfacets.
float NDF_TrowbridgeReitzGGX(in float cosLh, in float roughness)
{
	float alpha = roughness * roughness;
	float alphaSq = alpha * alpha;
	
	float denominator = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
	return alphaSq / (s_PI * denominator * denominator);
}

// GF: Geometry function
// Describes the self-shadowing property of the microfacets. 
// When a surface is relatively rough, the surface's microfacets can 
// overshadow other microfacets reducing the light the surface reflects.
// k: Remapping of a based on direct lighting ((a + 1)^2 / 8) or IBL lighting (a^2 / 2)
float GF_SchlickGGX(in float cosTheta, in float k)
{
	return cosTheta / (cosTheta * (1.0f - k) + k);
}

// Smiths geometry function takes both view direction (geometry obstruction) and the light direction vector (geometry shadowing)
// into account
float GF_Smith(in float cosLo, in float cosLi, in float roughness)
{
	float r = roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return GF_SchlickGGX(cosLo, k) * GF_SchlickGGX(cosLi, k);
}

// The Fresnel equation describes the ratio of surface reflection at different surface angles
float3 Fresnel_Schlick(in float cosTheta, in float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 Fresnel_SchlickRoughness(in float cosTheta, in float3 F0, in float roughness)
{
	return F0 + (max(1.0f - roughness, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 BRDF(in Material material, in float3 N, in float3 V, in float3 L, in float3 radiance)
{
	float3 F0 = lerp(Fdielectric, material.Albedo, material.Roughness);
	
	float3 H = normalize(L + V);
	
	float cosLo = saturate(dot(N, V)); // cosTheta for outgoing light into the camera
	float cosLi = saturate(dot(N, L)); // cosTheta for incoming light direction
	float cosLh = saturate(dot(N, H)); // cosTheta for half vector
	
	// CookTorrance BRDF
	float NDF = NDF_TrowbridgeReitzGGX(cosLh, material.Roughness);
	float3 F = Fresnel_Schlick(max(0.0f, dot(H, V)), F0);
	float GF = GF_Smith(cosLo, cosLi, material.Roughness);
	
	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
	float3 kD = lerp(1.0f - F, 0.0f, material.Roughness);
	
	// We don't scale by 1/PI for lighting & material units to be more convenient.
	// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	float3 diffuse = kD * material.Albedo/* / s_PI*/;
	float3 specular = (NDF * F * GF) / max(0.00001f, 4.0 * cosLi * cosLo); // specular, aka cook torrance brdf
	return (diffuse + specular) * radiance * cosLi;
}

float3 IBL(in Material material, in float cosLo, in float3 irradiance, in float3 prefiltered, in float2 BRDFIntegration)
{
	float3 F0 = lerp(Fdielectric, material.Albedo, material.Roughness);
	
	float3 F = Fresnel_SchlickRoughness(cosLo, F0, material.Roughness);
	float3 kD = lerp(1.0f - F, 0.0f, material.Roughness);

	float3 diffuse = kD * material.Albedo * irradiance;
	float3 specular = (F * BRDFIntegration.x + BRDFIntegration.y) * prefiltered;
	
	return diffuse + specular;
}
#endif