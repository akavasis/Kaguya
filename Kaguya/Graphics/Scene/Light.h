#ifndef __LIGHT_H__
#define __LIGHT_H__
#include "SharedDefines.h"

#ifdef __cplusplus
#include <array>
#include "Camera.h"
#endif

#define NUM_CASCADES 4
// Following HLSL packing rules
struct Cascade
{
	row_major matrix ShadowTransform;
	float4 Offset;
	float4 Scale;
	float Split;
	float3 _padding;
};

struct DirectionalLight
{
	float3 Strength;
	float Intensity;
	float3 Direction;
	float Lambda;
	Cascade Cascades[NUM_CASCADES];

#ifdef __cplusplus
	DirectionalLight();

	void RenderImGuiWindow();

	std::array<OrthographicCamera, NUM_CASCADES> GenerateCascades(const Camera& Camera, unsigned int Resolution);
#endif
};

struct PointLight
{
	float3 Strength;
	float Intensity;
	float3 Position;
	float Radius;

#ifdef __cplusplus
	PointLight();

	void RenderImGuiWindow();
#endif
};

struct SpotLight
{
	float3 Strength;
	float Intensity;
	float3 Position;
	float InnerConeAngle;
	float3 Direction;
	float OuterConeAngle;
	float Radius;
	float3 _padding;

#ifdef __cplusplus
	SpotLight();

	void RenderImGuiWindow();
#endif
};
#endif