#ifndef __SAMPLING_HLSLI__
#define __SAMPLING_HLSLI__

#include "Constants.hlsli"

// Section 13.6.1
float3 UniformSampleHemisphere(in float2 u)
{
	float z = u[0];
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2.0f * s_PI * u[1];
    return float3(r * cos(phi), r * sin(phi), z);
}

float UniformHemispherePdf()
{
    return s_1DIV2PI;
}

float3 UniformSampleSphere(in float2 u)
{
    float z = 1.0f - 2.0f * u[0];
    float r = sqrt(max(0.0f, 1.0f - z * z));
    float phi = 2.0f * s_PI * u[1];
    return float3(r * cos(phi), r * sin(phi), z);
}

float UniformSpherePdf()
{
    return s_1DIV4PI;
}

// Section 13.6.2
float2 UniformSampleDisk(in float2 u)
{
    float r = sqrt(u[0]);
    float theta = 2.0f * s_PI * u[1];
    return float2(r * cos(theta), r * sin(theta));
}

float2 ConcentricSampleDisk(in float2 u)
{
    float2 uOffset = 2.0f * u - float2(1, 1);
    if (uOffset.x == 0.0f && uOffset.y == 0.0f)
        return float2(0, 0);
    float theta, r;
    if (abs(uOffset.x) > abs(uOffset.y))
    {
        r = uOffset.x;
        theta = s_PIDIV4 * (uOffset.y / uOffset.x);
    }
    else
    {
        r = uOffset.y;
        theta = s_PIDIV2 - s_PIDIV4 * (uOffset.x / uOffset.y);
    }

    return r * float2(cos(theta), sin(theta));
}

// Section 13.6.3
float3 CosineSampleHemisphere(in float2 u)
{
    float2 d = ConcentricSampleDisk(u);
    float z = sqrt(max(0.0f, 1.0f - d.x * d.x - d.y * d.y));
    return float3(d.x, d.y, z);
}

float CosineHemispherePdf(float cosTheta)
{
    return cosTheta * s_1DIVPI;
}

// ONB: Orthonormal basis
struct ONB
{
    float3 tangent;
    float3 bitangent;
    float3 normal;

    float3 InverseTransform(in float3 p)
    {
        return p.x * tangent + p.y * bitangent + p.z * normal;
    }
};

ONB InitONB(in float3 normal)
{
    ONB onb;
    onb.normal = normalize(normal);
    if (abs(onb.normal.x) > abs(onb.normal.z))
    {
        onb.bitangent.x = -onb.normal.y;
        onb.bitangent.y = onb.normal.x;
        onb.bitangent.z = 0.0f;
    }
    else
    {
        onb.bitangent.x = 0.0f;
        onb.bitangent.y = -onb.normal.z;
        onb.bitangent.z = onb.normal.y;
    }

    onb.bitangent = normalize(onb.bitangent);
    onb.tangent = cross(onb.bitangent, onb.normal);

    return onb;
}

#endif