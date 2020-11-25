struct RenderPassData
{
    float4x4 World;
};
#define RenderPassDataType RenderPassData
#include <ShaderLayout.hlsli>

struct Vertex
{
    float4 PositionHS   : SV_Position;
    float3 Color       	: COLOR0;
};

float4 transformPos(float3 v)
{
	v = mul(float4(v, 1), g_RenderPassData.World).xyz;
	return mul(float4(v, 1), g_SystemConstants.Camera.ViewProjection);
}

static float3 cubeVertices[] = 
{
	float3(-0.5f, -0.5f, -0.5f),
	float3(-0.5f, -0.5f, 0.5f),
	float3(-0.5f, 0.5f, -0.5f),
	float3(-0.5f, 0.5f, 0.5f),
	float3(0.5f, -0.5f, -0.5f),
	float3(0.5f, -0.5f, 0.5f),
	float3(0.5f, 0.5f, -0.5f),
	float3(0.5f, 0.5f, 0.5f)
};

static float3 cubeColors[] =
{
	float3(0, 0, 0),
	float3(0, 0, 1),
	float3(0, 1, 0),
	float3(0, 1, 1),
	float3(1, 0, 0),
	float3(1, 0, 1),
	float3(1, 1, 0),
	float3(1, 1, 1)
};

static uint3 cubeIndices[] = 
{
	uint3(0, 2, 1),
	uint3(1, 2, 3),
	uint3(4, 5, 6),
	uint3(5, 7, 6),
	uint3(0, 1, 5),
	uint3(0, 5, 4),
	uint3(2, 6, 7),
	uint3(2, 7, 3),
	uint3(0, 4, 6),
	uint3(0, 6, 2),
	uint3(1, 3, 7),
	uint3(1, 7, 5)
};

[numthreads(12, 1, 1)]
[outputtopology("triangle")]
void MSMain(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID,
    out vertices Vertex outVerts[8],
    out indices uint3 outIndices[12]
)
{
	const uint numVertices = 8; // total of 8 verts
	const uint numPrimitives = 12; // 12 triangles, 2 on each side

    SetMeshOutputCounts(numVertices, numPrimitives);

	if (gtid < numVertices)
	{
		float3 pos = cubeVertices[gtid];
		outVerts[gtid].PositionHS = transformPos(pos);
		outVerts[gtid].Color = cubeColors[gtid];
	}

	outIndices[gtid] = cubeIndices[gtid];
}

float4 PSMain(Vertex input) : SV_TARGET
{
	return float4(input.Color, 1);
}