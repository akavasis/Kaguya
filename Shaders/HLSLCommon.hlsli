#ifndef __HLSL_COMMON_HLSLI__
#define __HLSL_COMMON_HLSLI__
#include "../Kaguya/Graphics/Scene/Light.h"
#include "../Kaguya/Graphics/Scene/SharedTypes.h"

static const float s_PI = 3.141592654f;
static const float s_2PI = 6.283185307f;
static const float s_1DIVPI = 0.318309886f;
static const float s_1DIV2PI = 0.159154943f;
static const float s_PIDIV2 = 1.570796327f;
static const float s_PIDIV4 = 0.785398163f;

half Luminance(half3 LinearColor)
{
	return dot(LinearColor, half3(0.3, 0.59, 0.11));
}

float3 CartesianToSpherical(float x, float y, float z)
{
	float radius = sqrt(x * x + y * y + z * z);
	float theta = acos(z / radius);
	float phi = atan(y / x);
	return float3(radius, theta, phi);
}

float3 SphericalToCartesian(float radius, float theta, float phi)
{
	float x = radius * sin(theta) * cos(phi);
	float y = radius * sin(theta) * sin(phi);
	float z = radius * cos(theta);
	return float3(x, y, z);
}

float CalcShadowRatio(in float3 WorldPosition, in int CascadeIndex, in Cascade Cascades[4], in SamplerComparisonState Sampler, in Texture2DArray ShadowMap)
{
	float4 shadowPosition = mul(float4(WorldPosition, 1.0f), Cascades[CascadeIndex].ShadowTransform);
	shadowPosition.xyz /= shadowPosition.w;
	float depth = shadowPosition.z;
		
	float2 shadowMapSize;
	float numSlices;
	ShadowMap.GetDimensions(shadowMapSize.x, shadowMapSize.y, numSlices);
		
	float dx = 1.0f / (float) shadowMapSize.x;
	const float2 offsets[9] =
	{
		float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx, +dx), float2(0.0f, +dx), float2(dx, +dx)
	};
		
	float shadowRatio = 0.0f;
		
		[unroll(9)]
	for (int i = 0; i < 9; ++i)
	{
		shadowRatio += ShadowMap.SampleCmpLevelZero(Sampler, float3(float2(shadowPosition.xy + offsets[i]), CascadeIndex), depth).r;
	}
		
	return shadowRatio / 9.0f;
}

float3 CalcDirectionalLightRadiance(uniform DirectionalLight directionalLight)
{
	return directionalLight.Strength * directionalLight.Intensity;
}

float3 CalcPointLightRadiance(uniform PointLight pointLight, float3 P)
{
	float3 pixelToLight = pointLight.Position - P;
	float distance = length(pixelToLight);
		
	if (distance > pointLight.Radius)
		return float3(0.0f, 0.0f, 0.0f);

	pixelToLight /= distance;
		
	float pointAttenuation = saturate(1.0f - distance * distance / (pointLight.Radius * pointLight.Radius));
	pointAttenuation *= pointAttenuation;
		
	return pointLight.Strength * pointLight.Intensity * pointAttenuation;
}

float3 CalcSpotLightRadiance(uniform SpotLight spotLight, float3 P)
{
	float3 pixelToLight = spotLight.Position - P;
	float distance = length(pixelToLight);
	pixelToLight /= distance;
		
	float pointAttenuation = saturate(1.0f - distance * distance / (spotLight.Radius * spotLight.Radius));
	pointAttenuation *= pointAttenuation;
		
	float spotRatio = saturate(dot(pixelToLight, spotLight.Direction));
	float spotAttenuation = saturate((spotRatio - spotLight.OuterConeAngle) / (spotLight.InnerConeAngle - spotLight.OuterConeAngle));
	spotAttenuation *= spotAttenuation;
		
	return spotLight.Strength * spotLight.Intensity * pointAttenuation * spotAttenuation;
}
#endif