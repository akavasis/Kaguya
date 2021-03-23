#ifndef SAMPLING_HLSLI
#define SAMPLING_HLSLI

#include <Math.hlsli>

float2 SampleUniformDisk(float2 Xi)
{
	float radius = sqrt(Xi[0]);
	float theta = g_2PI * Xi[1];
    
	return float2(radius * cos(theta), radius * sin(theta));
}

float2 SampleConcentricDisk(float2 Xi)
{
    // Map Xi to $[-1,1]^2$
	float2 XiOffset = 2.0f * Xi - 1.0f;

	// Handle degeneracy at the origin
	if (XiOffset.x == 0.0f && XiOffset.y == 0.0f)
	{
		return float2(0.0f, 0.0f);
	}

	// Apply concentric mapping to point
	float radius, theta;
	if (abs(XiOffset.x) > abs(XiOffset.y))
	{
		radius = XiOffset.x;
		theta = g_PIDIV4 * (XiOffset.y / XiOffset.x);
	}
	else
	{
		radius = XiOffset.y;
		theta = g_PIDIV2 - g_PIDIV4 * (XiOffset.x / XiOffset.y);
	}

	return float2(radius * cos(theta), radius * sin(theta));
}

float3 SampleUniformHemisphere(float2 Xi)
{
	float z = Xi[0];
	float r = sqrt(max(0.0f, 1.0f - z * z));
	float phi = 2.0f * g_PI * Xi[1];
	return float3(r * cos(phi), r * sin(phi), z);
}

float UniformHemispherePdf()
{
	return g_1DIV2PI;
}

float3 SampleCosineHemisphere(float2 Xi)
{
	float2 p = SampleConcentricDisk(Xi);
	float z = sqrt(max(0.0f, 1.0f - p.x * p.x - p.y * p.y));

	return float3(p.x, p.y, z);
}

float CosineHemispherePdf(float cosTheta)
{
	return cosTheta * g_1DIVPI;
}

float BalanceHeuristic(int nf, float fPdf, int ng, float gPdf)
{
	return (nf * fPdf) / (nf * fPdf + ng * gPdf);
}

float PowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{
	float f = nf * fPdf, g = ng * gPdf;
	return (f * f) / (f * f + g * g);
}

#endif // SAMPLING_HLSLI