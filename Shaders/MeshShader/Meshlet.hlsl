#include <HLSLCommon.hlsli>
#include <Vertex.hlsli>

struct Meshlet
{
	uint VertexCount;
	uint VertexOffset;
	uint PrimitiveCount;
	uint PrimitiveOffset;
};

struct VertexOut
{
    float4 PositionHS   : SV_Position;
    float3 Normal       : NORMAL;
    float2 TextureCoord	: TEXCOORD;
    uint   MeshletIndex : MESHLET_INDEX;
};

cbuffer RootConstants
{
	uint InstanceID;
};

StructuredBuffer<Mesh>    Meshes			  : register(t0, space0);

StructuredBuffer<Vertex>  Vertices            : register(t1, space0);
StructuredBuffer<Meshlet> Meshlets            : register(t2, space0);
ByteAddressBuffer         VertexIndices       : register(t3, space0);
StructuredBuffer<uint>    PrimitiveIndices    : register(t4, space0);

#include <ShaderLayout.hlsli>

/////
// Data Loaders

uint3 UnpackPrimitive(uint primitive)
{
    // Unpacks a 10 bits per index triangle from a 32-bit uint.
    return uint3(primitive & 0x3FF, (primitive >> 10) & 0x3FF, (primitive >> 20) & 0x3FF);
}

uint3 GetPrimitive(Meshlet m, uint index)
{
    return UnpackPrimitive(PrimitiveIndices[m.PrimitiveOffset + index]);
}

uint GetVertexIndex(Meshlet m, uint localIndex)
{
    localIndex = m.VertexOffset + localIndex;
    return VertexIndices.Load(localIndex * 4);
}

VertexOut GetVertexAttributes(uint meshletIndex, uint vertexIndex, Mesh mesh)
{
    Vertex v = Vertices[vertexIndex];

    float3 worldPos = mul(float4(v.Position, 1.0f), mesh.World).xyz;

    VertexOut vout;
	vout.PositionHS = mul(float4(worldPos, 1), g_SystemConstants.Camera.ViewProjection);
	vout.Normal = mul(float4(v.Normal, 0), mesh.World).xyz;
    vout.TextureCoord = v.Texture;
    vout.MeshletIndex = meshletIndex;

    return vout;
}

[numthreads(128, 1, 1)]
[outputtopology("triangle")]
void MSMain(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID,
    out indices uint3 tris[128],
    out vertices VertexOut verts[128]
)
{
    Mesh mesh = Meshes[InstanceID];
    Meshlet meshlet = Meshlets[gid];

    SetMeshOutputCounts(meshlet.VertexCount, meshlet.PrimitiveCount);

    if (gtid < meshlet.PrimitiveCount)
    {
        tris[gtid] = GetPrimitive(meshlet, gtid);
    }

    if (gtid < meshlet.VertexCount)
    {
        uint vertexIndex = GetVertexIndex(meshlet, gtid);
        verts[gtid] = GetVertexAttributes(gid, vertexIndex, mesh);
    }
}

float4 PSMain(VertexOut input) : SV_TARGET
{
    return float4(input.Normal, 0);
	uint meshletIndex = input.MeshletIndex;
	return float4(
        float(meshletIndex & 1),
        float(meshletIndex & 3) / 4,
        float(meshletIndex & 7) / 8,
        1);
}