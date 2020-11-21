#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#include <Vertex.hlsli>

struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

float BarycentricInterpolation(in float v0, in float v1, in float v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float2 BarycentricInterpolation(in float2 v0, in float2 v1, in float2 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float3 BarycentricInterpolation(in float3 v0, in float3 v1, in float3 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float4 BarycentricInterpolation(in float4 v0, in float4 v1, in float4 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

Vertex BarycentricInterpolation(in Vertex v0, in Vertex v1, in Vertex v2, in float3 barycentric)
{
    Vertex vertex;
    vertex.Position     = BarycentricInterpolation(v0.Position, v1.Position, v2.Position, barycentric);
    vertex.Texture      = BarycentricInterpolation(v0.Texture, v1.Texture, v2.Texture, barycentric);
    vertex.Normal       = BarycentricInterpolation(v0.Normal, v1.Normal, v2.Normal, barycentric);

    return vertex;
}

Vertex BarycentricInterpolation(in Triangle t, in float3 barycentric)
{
	return BarycentricInterpolation(t.v0, t.v1, t.v2, barycentric);
}

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates, using hlsl predefined struct
typedef BuiltInTriangleIntersectionAttributes HitAttributes;

#endif