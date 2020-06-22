#ifndef PBR_HLSLI
#define PBR_HLSLI
#include "HLSLCommon.hlsli"
static const float3 Fdielectric = 0.04;

// NDF: Normal distribution function
// Approximates the amount the surface's microfacets are aligned to the halfway vector, 
// influenced by the roughness of the surface.
// this is the primary function approximating the microfacets.
// N: Normal, H: Halfway, a: Roughness
float NDF_TrowbridgeReitzGGX(in float cosLh, in float Roughness)
{
	float alpha = Roughness * Roughness;
	float alphaSq = alpha * alpha;
	
	float denominator = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
	return alphaSq / (s_PI * denominator * denominator);
}

// GF: Geometry function
// Describes the self-shadowing property of the microfacets. 
// When a surface is relatively rough, the surface's microfacets can 
// overshadow other microfacets reducing the light the surface reflects.
// NdotV: Normal dot vector, k: Remapping of a based on direct lighting ((a + 1)^2 / 8) or IBL lighting (a^2 / 2)
float GF_SchlickGGX(in float cosTheta, in float k)
{
	return cosTheta / (cosTheta * (1.0f - k) + k);
}

// Smiths geometry function takes both view direction (geometry obstruction) and the light direction vector (geometry shadowing)
// into account
float GF_Smith(in float cosLo, in float cosLi, in float Roughness)
{
	float r = Roughness + 1.0;
	float k = (r * r) / 8.0; // Epic suggests using this roughness remapping for analytic lights.
	return GF_SchlickGGX(cosLo, k) * GF_SchlickGGX(cosLi, k);
}

// The Fresnel equation describes the ratio of surface reflection at different surface angles
float3 Fresnel_Schlick(in float cosTheta, in float3 F0)
{
	return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 Fresnel_SchlickRoughness(in float cosTheta, in float3 F0, in float Roughness)
{
	return F0 + (max(1.0f - Roughness, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float3 PBR(in MaterialSurfacePBR Surface, in float3 N, in float3 V, in float3 L, in float3 Radiance)
{
	float3 F0 = lerp(Fdielectric, Surface.Albedo, Surface.Metallic);
	
	float3 H = normalize(L + V);
	
	float cosLo = max(0.0f, dot(N, V)); // cosTheta for outgoing light into the camera
	float cosLi = max(0.0f, dot(N, L)); // cosTheta for incoming light direction
	float cosLh = max(0.0f, dot(N, H)); // cosTheta for half vector
	
	// CookTorrance BRDF
	float NDF = NDF_TrowbridgeReitzGGX(cosLh, Surface.Roughness);
	float3 F = Fresnel_Schlick(max(0.0f, dot(H, V)), F0);
	float GF = GF_Smith(cosLo, cosLi, Surface.Roughness);
	
	// Diffuse scattering happens due to light being refracted multiple times by a dielectric medium.
	// Metals on the other hand either reflect or absorb energy, so diffuse contribution is always zero.
	// To be energy conserving we must scale diffuse BRDF contribution based on Fresnel factor & metalness.
	float3 kD = lerp(1.0f - F, 0.0f, Surface.Metallic);
	
	// We don't scale by 1/PI for lighting & material units to be more convenient.
	// See: https://seblagarde.wordpress.com/2012/01/08/pi-or-not-to-pi-in-game-lighting-equation/
	float3 diffuse = kD * Surface.Albedo/* / s_PI*/;
	float3 specular = (NDF * F * GF) / max(0.00001f, 4.0 * cosLi * cosLo); // specular, aka cook torrance brdf
	return (diffuse + specular) * Radiance * cosLi;
}

float3 IBL(in MaterialSurfacePBR Surface, in float cosLo, in float3 Irradiance, in float3 Prefiltered, in float2 BRDFIntegration)
{
	float3 F0 = lerp(Fdielectric, Surface.Albedo, Surface.Metallic);
	
	float3 F = Fresnel_SchlickRoughness(cosLo, F0, Surface.Roughness);
	float3 kD = lerp(1.0f - F, 0.0f, Surface.Metallic);

	float3 diffuse = kD * Surface.Albedo * Irradiance;
	float3 specular = (F * BRDFIntegration.x + BRDFIntegration.y) * Prefiltered;
	
	return diffuse + specular;
}
#endif