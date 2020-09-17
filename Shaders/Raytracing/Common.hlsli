struct Vertex
{
    float3 Position;
    float2 Texture;
    float3 Normal;
    float3 Tangent;
    float3 Bitangent;
};

struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

// BERP: Short for barycentric interpolation
float BERP(in float v0, in float v1, in float v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float2 BERP(in float2 v0, in float2 v1, in float2 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float3 BERP(in float3 v0, in float3 v1, in float3 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float4 BERP(in float4 v0, in float4 v1, in float4 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

Vertex BERP(in Vertex v0, in Vertex v1, in Vertex v2, in float3 barycentric)
{
    Vertex vertex;
    vertex.Position     = BERP(v0.Position, v1.Position, v2.Position, barycentric);
    vertex.Texture      = BERP(v0.Texture, v1.Texture, v2.Texture, barycentric);
    vertex.Normal       = BERP(v0.Normal, v1.Normal, v2.Normal, barycentric);
    vertex.Tangent      = BERP(v0.Tangent, v1.Tangent, v2.Tangent, barycentric);
    vertex.Bitangent    = BERP(v0.Bitangent, v1.Bitangent, v2.Bitangent, barycentric);

    return vertex;
}

Vertex BERP(in Triangle t, in float3 barycentric)
{
    return BERP(t.v0, t.v1, t.v2, barycentric);
}

// Hit information, aka ray payload
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct RayPayload
{
    float3 Radiance;
	float3 Throughput;
	uint Seed;
	uint Depth;
};

struct ShadowRayPayload
{
    float Visibility;
};

// Attributes output by the raytracing when hitting a surface,
// here the barycentric coordinates, using hlsl predefined struct
typedef BuiltInTriangleIntersectionAttributes HitAttributes;

enum RayType
{
    RayTypePrimary,
  RayTypeShadow,
  NumRayTypes
};