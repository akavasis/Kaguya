#include "../LTC.hlsli"
#include "../GBuffer.hlsli"

struct ShadingData
{
	int Position;
	int Normal;
	int Albedo;
	int TypeAndIndex;
	int DepthStencil;
	
	int LTC_LUT_DisneyDiffuse_InverseMatrix;
	int LTC_LUT_DisneyDiffuse_Terms;
	int LTC_LUT_GGX_InverseMatrix;
	int LTC_LUT_GGX_Terms;
	
	int RenderTarget;
};
#define RenderPassDataType ShadingData
#include "Global.hlsli"

struct ShadingResult
{
	float3 AnalyticUnshadowed;
	float3 StochasticUnshadowed;
};

void InitShadingResult(out ShadingResult shadingResult)
{
	shadingResult.AnalyticUnshadowed = 0.0.xxx;
	shadingResult.StochasticUnshadowed = 0.0.xxx;
}

float3x3 inverse(float3x3 m)
{
	// computes the inverse of a matrix m
	float det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2]) -
             m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
             m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);

	float invdet = 1 / det;

	float3x3 minv; // inverse of matrix m
	minv[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * invdet;
	minv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * invdet;
	minv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * invdet;
	minv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * invdet;
	minv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * invdet;
	minv[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * invdet;
	minv[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * invdet;
	minv[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * invdet;
	minv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * invdet;
	return minv;
}

void GetLTCTerms(float3 N, float3 V, float roughness, out LTCTerms ltcTerms)
{
	float NoV = saturate(dot(N, V));
	float2 uv = GetLTC_LUT_UV(NoV, roughness);
	
	Texture2D LTC_LUT_GGX_InverseMatrix = g_Texture2DTable[g_RenderPassData.LTC_LUT_GGX_InverseMatrix];
	Texture2D LTC_LUT_GGX_Terms = g_Texture2DTable[g_RenderPassData.LTC_LUT_GGX_Terms];
	
	float3x3 Minv = GetLTC_LUT_InverseMatrix(LTC_LUT_GGX_InverseMatrix, SamplerLinearClamp, uv);
	float4 terms = GetLTC_LUT_Terms(LTC_LUT_GGX_Terms, SamplerLinearClamp, uv);
	
	// Construct orthonormal basis around N
	float3 T1, T2;
	T1 = normalize(V - N * dot(V, N));
	T2 = cross(N, T1);
	float3x3 w2t = transpose(float3x3(T1, T2, N));
	
	ltcTerms.MinvS = mul(w2t, Minv);
	ltcTerms.MS = inverse(ltcTerms.MinvS);
	ltcTerms.MdetS = determinant(ltcTerms.MS);

	ltcTerms.MinvD = w2t;
	ltcTerms.MD = transpose(ltcTerms.MinvD);
	ltcTerms.MdetD = 1.0f;
	
	ltcTerms.Magnitude = terms.x;
	ltcTerms.Fresnel = terms.y;
}

float3 ShadeWithAreaLight(float3 P, float3 diffuse, float3 specular, LTCTerms ltcTerms, float3 points[4])
{
	// Apply BRDF magnitude and Fresnel
	specular = specular * ltcTerms.Magnitude + (1.0.xxx - specular) * ltcTerms.Fresnel;
	
	// BRDF shadowing and fresnel
	float specularLobe = LTC_Evaluate(P, ltcTerms.MinvS, points, true);
	specular *= specularLobe;
	
	float diffuseLobe = LTC_Evaluate(P, ltcTerms.MinvD, points, true);
	diffuse *= diffuseLobe;
	
	return specular + diffuse;
}

struct RayPayload
{
	bool Dummy;
};

struct ShadowRayPayload
{
	float Visibility; // 1.0f for fully lit, 0.0f for fully shadowed
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

float TraceShadowRay(float3 Origin, float TMin, float3 Direction, float TMax)
{
	RayDesc ray = { Origin, TMin, Direction, TMax };
	ShadowRayPayload rayPayload = { 0.0f };

	// Trace the ray
	const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = 0;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = 0;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);

	return rayPayload.Visibility;
}

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(g_SystemConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions);
	const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);
	
	ShadingResult shadingResult;
	InitShadingResult(shadingResult);
	
	GBuffer GBuffer;
	GBuffer.Position = g_Texture2DTable[g_RenderPassData.Position];
	GBuffer.Normal = g_Texture2DTable[g_RenderPassData.Normal];
	GBuffer.Albedo = g_Texture2DTable[g_RenderPassData.Albedo];
	GBuffer.TypeAndIndex = g_Texture2DUINT4Table[g_RenderPassData.TypeAndIndex];
	GBuffer.DepthStencil = g_Texture2DTable[g_RenderPassData.DepthStencil];
	
	uint GBufferType = GetGBufferType(GBuffer, launchIndex);
	
	float3 color = float3(0.0f, 0.0f, 0.0f);
	if (GBufferType == GBufferTypeMesh)
	{
		GBufferMesh GBufferMesh = GetGBufferMesh(GBuffer, launchIndex);
		
		float3 position = GBufferMesh.Position;
		float3 normal = GBufferMesh.Normal;
		float3 viewDir = normalize(g_SystemConstants.Camera.Position.xyz - position);
		
		LTCTerms ltcTerms;
		GetLTCTerms(normal, viewDir, GBufferMesh.Roughness, ltcTerms);
		
		float3 specular = lerp(float3(0.04f, 0.04f, 0.04f), GBufferMesh.Albedo, GBufferMesh.Metallic);
		float3 diffuse = GBufferMesh.Albedo;
	
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
			
			// float Visibility = TraceShadowRay(position, 0.001f, )

			color += (Light.Color * Light.Luminance) * (ShadeWithAreaLight(position, diffuse, specular, ltcTerms, points));
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

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayPayload)
{
	// If we miss all geometry, then the light is visibile
	rayPayload.Visibility = 1.0f;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayPayload, in HitAttributes attrib)
{
}