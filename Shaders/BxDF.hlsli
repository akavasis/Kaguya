#ifndef __BXDF_HLSLI__
#define __BXDF_HLSLI__

#include "Constants.hlsli"

// NDF: Normal distribution function
// Approximates the amount the surface's microfacets are aligned to the halfway vector, 
// influenced by the roughness of the surface.
// this is the primary function approximating the microfacets.
float D_TrowbridgeReitzGGX(float NoH, float roughness)
{
	float a2 = roughness * roughness;
	float d = ((NoH * a2 - NoH) * NoH + 1);
	return a2 / (s_PI * d * d);
}

// GF: Geometry function
// Describes geometric masking of the microfacets. I.e., facets of various orientations will not always be visible; 
// they may get occluded by other tiny facets. The model for geometric masking we use is from Schlick's BRDF model (or direct PDF).
// https://onlinelibrary.wiley.com/doi/abs/10.1111/1467-8659.1330233
float G_Schlick(float NoL, float NoV, float roughness)
{
	float k = roughness * roughness / 2.0f;
	
	float g_l = NoL / (NoL * (1.0f - k) + k);
	float g_v = NoV / (NoV * (1.0f - k) + k);
	
	return g_l * g_v;
}

// The Fresnel equation describes the ratio of surface reflection at different surface angles
float3 F_Schlick(float cosTheta, float3 f0)
{
	return f0 + (1.0f - f0) * pow(1.0f - cosTheta, 5.0f);
}

float F_Schlick(float cosTheta, float IndexOfRefraction)
{
	float3 f0 = ((1.0f - IndexOfRefraction) / (1.0f + IndexOfRefraction)).xxx;
	f0 = f0 * f0;
	return F_Schlick(cosTheta, f0).r;
}

float3 BRDF(float3 N, float3 V, float3 H, float3 L, float3 albedo, float3 specular, float roughness, float3 radiance)
{
	// L will be facing towards the same direction as normal
	float NoL = saturate(dot(N, L));
	float NoH = saturate(dot(N, H));
	float LoH = saturate(dot(L, H));
	float NoV = saturate(dot(N, V));
	
	// CookTorrance BRDF
	float D = D_TrowbridgeReitzGGX(NoH, roughness);
	float G = G_Schlick(NoL, NoV, roughness);
	float3 F = F_Schlick(LoH, specular);
	
	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
	float3 kD = lerp(1.0f - F, 0.0f, roughness);
	
	float3 Fd = kD * albedo / s_PI;
	float3 Fs = (D * F * G) / (4.0 * NoL * NoV + 0.00001f); // specular, aka cook torrance brdf
	return (Fd + Fs) * radiance * NoL;
}

#endif