#include "GBuffer.hlsli"
#include "LTC.hlsli"

StructuredBuffer<PolygonalLight>	Lights		: register(t0, space0);
StructuredBuffer<Material>			Materials	: register(t1, space0);

struct LTCData
{
	GlobalConstants GlobalConstants;
	int Position;
	int Normal;
	int Albedo;
	int TypeAndIndex;
	int DepthStencil;
	
	int LTCLUT1;
	int LTCLUT2;
};
#define RenderPassDataType LTCData
#include "ShaderLayout.hlsli"

SamplerState LinearClamp : register(s0);

#include "Quad.hlsl"

float4 PSMain(VSOutput IN) : SV_Target
{
	uint2 pixel = uint2(IN.Position.xy + 0.5f);
	uint3 location = uint3(pixel, 0);
	
	GBuffer GBuffer;
	GBuffer.Position = Texture2DTable[RenderPassData.Position];
	GBuffer.Normal = Texture2DTable[RenderPassData.Normal];
	GBuffer.Albedo = Texture2DTable[RenderPassData.Albedo];
	GBuffer.TypeAndIndex = Texture2DUINT4Table[RenderPassData.TypeAndIndex];
	GBuffer.DepthStencil = Texture2DTable[RenderPassData.DepthStencil];
	
	uint GBufferType = GetGBufferType(GBuffer, pixel);
	
	float3 color = float3(0.0f, 0.0f, 0.0f);
	if (GBufferType == GBufferTypeMesh)
	{
		GBufferMesh GBufferMesh = GetGBufferMesh(GBuffer, pixel);
		Material Material = Materials[GBufferMesh.MaterialIndex];
		
		// Only evaluating 1 light for now
		PolygonalLight Light = Lights[0];
		
		Texture2D LTCLUT1 = Texture2DTable[RenderPassData.LTCLUT1];
		Texture2D LTCLUT2 = Texture2DTable[RenderPassData.LTCLUT2];
	
		float3 position = GBufferMesh.Position;
		float3 normal = GBufferMesh.Normal;
		float3 albedo = GBufferMesh.Albedo;
		float roughness = GBufferMesh.Roughness;
	
		float halfWidth = Light.Width * 0.5f;
		float halfHeight = Light.Height * 0.5f;
		// Get billboard points at the origin
		float3 p0 = float3(halfWidth, -halfHeight, 0.0);
		float3 p1 = float3(halfWidth, halfHeight, 0.0);
		float3 p2 = float3(-halfWidth, halfHeight, 0.0);
		float3 p3 = float3(-halfWidth, -halfHeight, 0.0);
		
		p0 = mul(p0, (float3x3) Light.World);
		p1 = mul(p1, (float3x3) Light.World);
		p2 = mul(p2, (float3x3) Light.World);
		p3 = mul(p3, (float3x3) Light.World);
		
		// Move points to light's location
		// Clockwise to match LTC convention
		float3 points[4];
		points[0] = p3 + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);
		points[1] = p2 + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);
		points[2] = p1 + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);
		points[3] = p0 + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);

		float3 viewDir = normalize(RenderPassData.GlobalConstants.EyePosition - position);

		float NoV = saturate(dot(normal, viewDir));
		float2 uv = float2(roughness, sqrt(1.0f - NoV));
		uv = uv * LUT_SCALE + LUT_BIAS;

		float4 ltcLookupA = LTCLUT1.SampleLevel(LinearClamp, uv, 0.0f);
		float4 ltcLookupB = LTCLUT2.SampleLevel(LinearClamp, uv, 0.0f);
		float ltcMagnitude = ltcLookupB.y;
		float ltcFresnel = ltcLookupB.z;
	
		float3x3 identity = float3x3
		(
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		);

		float3 r1 = float3(ltcLookupA.x, 0, ltcLookupA.y);
		float3 r2 = float3(0, 1.0f, 0);
		float3 r3 = float3(ltcLookupA.z, 0, ltcLookupA.w);
		float3x3 Minv = float3x3(r1, r2, r3);

		float3 diff = LTC_Evaluate(normal, viewDir, position, identity, points, false);
		float3 spec = LTC_Evaluate(normal, viewDir, position, Minv, points, false);
		spec *= ltcLookupB.x + (1.0f - Material.Specular) * ltcLookupB.y;

		color = (Light.Color * Light.Intensity) * (albedo * diff + spec);
	}
	else if (GBufferType == GBufferTypeLight)
	{
		GBufferLight GBufferLight = GetGBufferLight(GBuffer, pixel);
		PolygonalLight Light = Lights[GBufferLight.LightIndex];
		color = Light.Color * Light.Intensity;
	}
	
	return float4(color, 1.0f);
}