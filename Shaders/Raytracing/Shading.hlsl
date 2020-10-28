#include "../LTC.hlsli"
#include "../GBuffer.hlsli"

struct ShadingData
{
	int Position;
	int Normal;
	int Albedo;
	int TypeAndIndex;
	int DepthStencil;
	
	int LTC_LUT_GGX_InverseMatrix;
	int LTC_LUT_GGX_Terms;
	
	int RenderTarget;
};
#define RenderPassDataType ShadingData
#include "Global.hlsli"

struct RayPayload
{
	bool Dummy;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(g_SystemConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions);
	const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);
	
	GBuffer GBuffer;
	GBuffer.Position		= g_Texture2DTable[g_RenderPassData.Position];
	GBuffer.Normal			= g_Texture2DTable[g_RenderPassData.Normal];
	GBuffer.Albedo			= g_Texture2DTable[g_RenderPassData.Albedo];
	GBuffer.TypeAndIndex	= g_Texture2DUINT4Table[g_RenderPassData.TypeAndIndex];
	GBuffer.DepthStencil	= g_Texture2DTable[g_RenderPassData.DepthStencil];
	
	uint GBufferType = GetGBufferType(GBuffer, launchIndex);
	
	float3 color = float3(0.0f, 0.0f, 0.0f);
	if (GBufferType == GBufferTypeMesh)
	{
		GBufferMesh GBufferMesh = GetGBufferMesh(GBuffer, launchIndex);
		
		Texture2D LTC_LUT_GGX_InverseMatrix = g_Texture2DTable[g_RenderPassData.LTC_LUT_GGX_InverseMatrix];
		Texture2D LTC_LUT_GGX_Terms = g_Texture2DTable[g_RenderPassData.LTC_LUT_GGX_Terms];
		
		float3 position = GBufferMesh.Position;
		float3 normal = GBufferMesh.Normal;
		float3 albedo = GBufferMesh.Albedo;
		float roughness = GBufferMesh.Roughness;
		float metallic = GBufferMesh.Metallic;
		
		float3 viewDir = normalize(g_SystemConstants.Camera.Position.xyz - position);
		
		float NoV = saturate(dot(normal, viewDir));
		float2 LTCUV = GetLTCUV(NoV, roughness);
		float3x3 LTCInverseMatrix = GetLTCInverseMatrix(LTC_LUT_GGX_InverseMatrix, SamplerLinearClamp, LTCUV);
		float4 LTCTerms = GetLTCTerms(LTC_LUT_GGX_Terms, SamplerLinearClamp, LTCUV);
		
		float magnitude = LTCTerms.x;
		float fresnel = LTCTerms.y;
		
		float3 f90 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
		float3 specular = f90 * magnitude + (1.0f - f90) * fresnel;
	
		float3x3 identity = float3x3
		(
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		);

		for (int i = 0; i < g_SystemConstants.NumPolygonalLights; ++i)
		{
			PolygonalLight Light = Lights[i];
			
			float halfWidth = Light.Width * 0.5f;
			float halfHeight = Light.Height * 0.5f;
			// Get billboard points at the origin
			float3 p0 = float3(+halfWidth, -halfHeight, 0.0);
			float3 p1 = float3(+halfWidth, +halfHeight, 0.0);
			float3 p2 = float3(-halfWidth, +halfHeight, 0.0);
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

			float3 diff = albedo / s_PI;
			diff *= LTC_Evaluate(normal, viewDir, position, identity, points, false);
			
			// BRDF shadowing and fresnel
			float3 spec = specular;
			spec *= LTC_Evaluate(normal, viewDir, position, LTCInverseMatrix, points, false);

			color += (Light.Color * Light.Luminance) * (diff + spec);
		}
	}
	else if (GBufferType == GBufferTypeLight)
	{
		GBufferLight GBufferLight = GetGBufferLight(GBuffer, launchIndex);
		PolygonalLight Light = Lights[GBufferLight.LightIndex];
		color = (Light.Color * Light.Luminance);
	}
	
	color = ExposeLuminance(color, g_SystemConstants.Camera);
	
	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[g_RenderPassData.RenderTarget];
	RenderTarget[launchIndex] = float4(color, 1.0f);
}