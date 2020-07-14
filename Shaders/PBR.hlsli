#ifndef __PBR_HLSLI__
#define __PBR_HLSLI__
#include "HLSLCommon.hlsli"
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

float3 PBR(in MaterialSurface surface, in float3 N, in float3 V, in float3 L, in float3 radiance)
{
	float3 F0 = lerp(Fdielectric, surface.Albedo, surface.Metallic);
	
	float3 H = normalize(L + V);
	
	float cosLo = max(0.0f, dot(N, V)); // cosTheta for outgoing light into the camera
	float cosLi = max(0.0f, dot(N, L)); // cosTheta for incoming light direction
	float cosLh = max(0.0f, dot(N, H)); // cosTheta for half vector
	
	// CookTorrance BRDF
	float NDF = NDF_TrowbridgeReitzGGX(cosLh, surface.Roughness);
	float3 F = Fresnel_Schlick(max(0.0f, dot(H, V)), F0);
	float GF = GF_Smith(cosLo, cosLi, surface.Roughness);
	
	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
	float3 kD = lerp(1.0f - F, 0.0f, surface.Metallic);
	
	// We don't scale by 1/PI for lighting & material units to be more convenient.
	// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	float3 diffuse = kD * surface.Albedo/* / s_PI*/;
	float3 specular = (NDF * F * GF) / max(0.00001f, 4.0 * cosLi * cosLo); // specular, aka cook torrance brdf
	return (diffuse + specular) * radiance * cosLi;
}

float3 IBL(in MaterialSurface surface, in float cosLo, in float3 irradiance, in float3 prefiltered, in float2 BRDFIntegration)
{
	float3 F0 = lerp(Fdielectric, surface.Albedo, surface.Metallic);
	
	float3 F = Fresnel_SchlickRoughness(cosLo, F0, surface.Roughness);
	float3 kD = lerp(1.0f - F, 0.0f, surface.Metallic);

	float3 diffuse = kD * surface.Albedo * irradiance;
	float3 specular = (F * BRDFIntegration.x + BRDFIntegration.y) * prefiltered;
	
	return diffuse + specular;
}
#endif