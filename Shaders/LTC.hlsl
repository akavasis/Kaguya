#include "GBuffer.hlsli"

StructuredBuffer<PolygonalLight> Lights : register(t0, space0);

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

static const float LUT_SIZE = 64.0f;
static const float LUT_SCALE = (LUT_SIZE - 1.0f) / LUT_SIZE;
static const float LUT_BIAS = 0.5 / LUT_SIZE;

float IntegrateEdge(float3 v1, float3 v2)
{
	float x = dot(v1, v2);
	float y = abs(x);

	float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
	float b = 3.4175940 + (4.1616724 + y) * y;
	float v = a / b;

	float theta_sintheta = (x > 0.0) ? v : 0.5 * rsqrt(1.0 - x * x) - v;

	return (cross(v1, v2) * theta_sintheta).z;
}

void ClipQuadToHorizon(inout float3 L[5], out int n)
{
    // detect clipping config
	int config = 0;
	if (L[0].z > 0.0)
		config += 1;
	if (L[1].z > 0.0)
		config += 2;
	if (L[2].z > 0.0)
		config += 4;
	if (L[3].z > 0.0)
		config += 8;

    // clip
	n = 0;

	if (config == 0)
	{
        // clip all
	}
	else if (config == 1) // V1 clip V2 V3 V4
	{
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 2) // V2 clip V1 V3 V4
	{
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 3) // V1 V2 clip V3 V4
	{
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 4) // V3 clip V1 V2 V4
	{
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	}
	else if (config == 5) // V1 V3 clip V2 V4) impossible
	{
		n = 0;
	}
	else if (config == 6) // V2 V3 clip V1 V4
	{
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 7) // V1 V2 V3 clip V4
	{
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 8) // V4 clip V1 V2 V3
	{
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = L[3];
	}
	else if (config == 9) // V1 V4 clip V2 V3
	{
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	}
	else if (config == 10) // V2 V4 clip V1 V3) impossible
	{
		n = 0;
	}
	else if (config == 11) // V1 V2 V4 clip V3
	{
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 12) // V3 V4 clip V1 V2
	{
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	}
	else if (config == 13) // V1 V3 V4 clip V2
	{
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	}
	else if (config == 14) // V2 V3 V4 clip V1
	{
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	}
	else if (config == 15) // V1 V2 V3 V4
	{
		n = 4;
	}

	if (n == 3)
		L[3] = L[0];
	if (n == 4)
		L[4] = L[0];
}

float3 LTC_Evaluate(float3 N, float3 V, float3 P, float3x3 Minv, float3 points[4], bool twoSided)
{
    // construct orthonormal basis around N
	float3 T1, T2;
	T1 = normalize(V - N * dot(V, N));
	T2 = cross(N, T1);

    // rotate area light in (T1, T2, N) basis
	Minv = mul(Minv, transpose(float3x3(T1, T2, N)));

    // polygon (allocate 5 vertices for clipping)
	float3 L[5];
	L[0] = mul(Minv, (points[0] - P));
	L[1] = mul(Minv, (points[1] - P));
	L[2] = mul(Minv, (points[2] - P));
	L[3] = mul(Minv, (points[3] - P));

	int n;
	ClipQuadToHorizon(L, n);

	if (n == 0)
		return float3(0, 0, 0);

	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	float sum = 0.0;
	sum += IntegrateEdge(L[0], L[1]);
	sum += IntegrateEdge(L[1], L[2]);
	sum += IntegrateEdge(L[2], L[3]);

	if (n >= 4)
		sum += IntegrateEdge(L[3], L[4]);

	if (n == 5)
		sum += IntegrateEdge(L[4], L[0]);

	sum = twoSided ? abs(sum) : max(0.0, sum);

	float3 Lo_i = float3(sum, sum, sum);

	return Lo_i;
}

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
		
		// Only evaluating 1 light
		PolygonalLight Light = Lights[0];
		
		Texture2D LTCLUT1 = Texture2DTable[RenderPassData.LTCLUT1];
		Texture2D LTCLUT2 = Texture2DTable[RenderPassData.LTCLUT2];
	
		const float roughness = 0.25;
		const float metalness = 0.25;
		const float intensity = 4;
		float3 position = GBufferMesh.Position;
		float3 normal = GBufferMesh.Normal;
		float3 albedo = GBufferMesh.Albedo;
	
		float3 points[4];
		float hw = 0.5 * 1.0;
		float hh = 0.5 * 1.0;
		matrix worldView = Light.World * RenderPassData.GlobalConstants.View;
		points[0] = mul(float4(-hw, -hh, 0, 1.0), worldView).xyz;
		points[1] = mul(float4(-hw, +hh, 0, 1.0), worldView).xyz;
		points[2] = mul(float4(hw, +hh, 0, 1.0), worldView).xyz;
		points[3] = mul(float4(hw, -hh, 0, 1.0), worldView).xyz;

		float3 viewDir = normalize(RenderPassData.GlobalConstants.EyePosition - position);

		float ndotv = clamp(dot(normal, viewDir), 0, 1);
		float2 uv = float2(roughness, sqrt(1 - ndotv));
		uv = uv * LUT_SCALE + LUT_BIAS;

		float4 ltcLookupA = LTCLUT1.SampleLevel(LinearClamp, uv, 0.0f);
		float3 ltcLookupB = LTCLUT2.SampleLevel(LinearClamp, uv, 0.0f).rgb;
		float ltcMagnitude = ltcLookupB.y;
		float ltcFresnel = ltcLookupB.z;
	
		float3x3 identity = float3x3
		(
			1, 0, 0,
			0, 1, 0,
			0, 0, 1
		);

		float3x3 Minv = float3x3
		(
			float3(ltcLookupA.x, 0, ltcLookupA.y),
			float3(0, ltcLookupA.z, 0),
			float3(ltcLookupA.w, 0, ltcLookupB.x)
		);

		float3 diff = LTC_Evaluate(normal, viewDir, position, identity, points, false);

		float LightArea = 1;
		float3 lightColor = intensity / LightArea / (2 * s_PI * s_PI);

		float3 spec = LTC_Evaluate(normal, viewDir, position, Minv, points, false);
		spec *= albedo * ltcMagnitude + (1 - albedo) * ltcFresnel;

		// spec is not multiplied by metalness because conductors also reflect light
		color = lightColor * (spec + (1 - metalness) * albedo * diff);

	}
	else if (GBufferType == GBufferTypeLight)
	{
		GBufferLight GBufferLight = GetGBufferLight(GBuffer, pixel);
		PolygonalLight Light = Lights[GBufferLight.LightIndex];
		color = Light.Color;
	}
	
	return float4(color, 1.0f);
}