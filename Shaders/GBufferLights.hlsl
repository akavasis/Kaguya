#include <GBuffer.hlsli>

struct VertexAttributes
{
	float4 PositionH 	: SV_POSITION;
	float3 PositionW 	: POSITION;
	uint   LightIndex 	: LIGHT_INDEX;
};

//----------------------------------------------------------------------------------------------------
StructuredBuffer<PolygonalLight> Lights : register(t0, space0);

#include <ShaderLayout.hlsli>

uint3 GetPrimitive(uint index)
{
	uint3 primitives[] =
	{
		uint3(0, 1, 2),
		uint3(0, 2, 3)
	};

    return primitives[index];
}

VertexAttributes GetVertexAttributes(uint LightIndex, uint VertexIndex, PolygonalLight Light)
{
    VertexAttributes VertexAttributes;

	float w2 = Light.Width * 0.5f;
	float h2 = Light.Height * 0.5f;

	float2 vertices[] = 
	{
		float2(-w2, -h2),
		float2(+w2, -h2),
		float2(+w2, +h2),
		float2(-w2, +h2)
	};

	float3 lightPoint = float3(vertices[VertexIndex], 0.0f);

	// Rotate around origin
	lightPoint = mul(lightPoint, (float3x3) Light.World);

	// Move points to light's location
	VertexAttributes.PositionW = lightPoint + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);
		
	// Transform to homogeneous/clip space
	VertexAttributes.PositionH = mul(float4(VertexAttributes.PositionW, 1.0f), g_SystemConstants.Camera.ViewProjection);

	VertexAttributes.LightIndex = LightIndex;

    return VertexAttributes;
}

[numthreads(4, 1, 1)]
[outputtopology("triangle")]
void MSMain(uint gtid : SV_GroupThreadID, uint gid : SV_GroupID,
    out indices uint3 tris[2],
    out vertices VertexAttributes verts[4]
)
{
	const uint VertexCount = 4;
	const uint PrimitiveCount = 2;

    SetMeshOutputCounts(VertexCount, PrimitiveCount);

	PolygonalLight Light = Lights[gid];

    // SV_GroupThreadID is always relative to numthreads attribute, so we can use that to
    // identify our primitives and vertices
    if (gtid < PrimitiveCount)
    {
        tris[gtid] = GetPrimitive(gtid);
    }

    if (gtid < VertexCount)
    {
        verts[gtid] = GetVertexAttributes(gid, gtid, Light);
    }
}

MRT PSMain(VertexAttributes IN)
{
	GBufferLight light = (GBufferLight) 0;
	light.LightIndex = IN.LightIndex;
	
	MRT OUT;
	OUT.Albedo				= 0.0.xxxx;
	OUT.Normal				= 0.0.xxxx;
	OUT.TypeAndIndex		= (GBufferTypeLight << 4) | (light.LightIndex & 0x0000000F);
	OUT.SVGF_LinearZ		= 0.0.xxxx;
	OUT.SVGF_MotionVector	= 0.0.xxxx;
	OUT.SVGF_Compact		= 0.0.xxxx;
	return OUT;
}