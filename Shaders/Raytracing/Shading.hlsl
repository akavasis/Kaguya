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
	int BlueNoise;

	int AnalyticUnshadowed;
	int StochasticUnshadowed;
	int StochasticShadowed;
};
#define RenderPassDataType ShadingData
#include "Global.hlsli"

struct ShadingResult
{
	float3 AnalyticUnshadowed;
	float3 StochasticUnshadowed;
	float3 StochasticShadowed;
};

ShadingResult InitShadingResult()
{
	ShadingResult Out;
	Out.AnalyticUnshadowed		= 0.0.xxx;
	Out.StochasticUnshadowed	= 0.0.xxx;
	Out.StochasticShadowed		= 0.0.xxx;
	return Out;
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

float3 ShadeWithAreaLight(float3 P, float3 diffuse, float3 specular, LTCTerms ltcTerms, float3 points[4], out float diffProb, out float brdfProb)
{
	// Apply BRDF magnitude and Fresnel
	specular = specular * ltcTerms.Magnitude + (1.0.xxx - specular) * ltcTerms.Fresnel;
	
	// BRDF shadowing and fresnel
	float specularLobe = LTC_Evaluate(P, ltcTerms.MinvS, points);
	specular *= specularLobe;
	
	float diffuseLobe = LTC_Evaluate(P, ltcTerms.MinvD, points);
	diffuse *= diffuseLobe;
	
	float specMag = RGBToCIELuminance(specular);
	float diffMag = RGBToCIELuminance(diffuse);
	
	diffProb = diffMag / max(diffMag + specMag, 1e-5);
	brdfProb = lerp(specularLobe, diffuseLobe, diffProb);
	
	return specular + diffuse;
}

float3 BRDFEval(float3 diffuse, float3 specular, LTCTerms ltcTerms, float3 w_i, float diffProb, out float pdf)
{
	float specPDF = LTC_Sample_PDF(ltcTerms.MinvS, ltcTerms.MdetS, w_i);
	float diffPDF = LTC_Sample_PDF(ltcTerms.MinvD, ltcTerms.MdetD, w_i);

    // (Could be precomputed)
	specular = specular * ltcTerms.Magnitude + (1.0.xxx - specular) * ltcTerms.Fresnel;

	pdf = lerp(specPDF, diffPDF, diffProb);

	return specular * specPDF + diffuse * diffPDF;
}

float3 BRDFSample(bool bSpec, float u1, float u2, LTCTerms ltcTerms)
{
	float3x3 m = bSpec ? ltcTerms.MS : ltcTerms.MD;
	return LTC_Sample_Vector(m, u1, u2);
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
	const uint hitGroupIndexMultiplier = 0;
	const uint missShaderIndex = 0;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);

	return rayPayload.Visibility;
}

[shader("raygeneration")]
void RayGeneration()
{
	// Distance bias to avoid self-shadowing
	const float epsilon = 0.005f;
	
	const uint2 launchIndex = DispatchRaysIndex().xy;
	
	ShadingResult shadingResult = InitShadingResult();
	
	GBuffer GBuffer;
	GBuffer.Position = g_Texture2DTable[g_RenderPassData.Position];
	GBuffer.Normal = g_Texture2DTable[g_RenderPassData.Normal];
	GBuffer.Albedo = g_Texture2DTable[g_RenderPassData.Albedo];
	GBuffer.TypeAndIndex = g_Texture2DUINT4Table[g_RenderPassData.TypeAndIndex];
	GBuffer.DepthStencil = g_Texture2DTable[g_RenderPassData.DepthStencil];
	
	uint GBufferType = GetGBufferType(GBuffer, launchIndex);
	
	if (GBufferType == GBufferTypeMesh)
	{
		GBufferMesh GBufferMesh = GetGBufferMesh(GBuffer, launchIndex);
		
		float3 position = GBufferMesh.Position;
		float3 normal = GBufferMesh.Normal;
		float3 viewDir = normalize(g_SystemConstants.Camera.Position.xyz - position);
		
		LTCTerms ltcTerms;
		GetLTCTerms(normal, viewDir, GBufferMesh.Roughness, ltcTerms);
		Texture2D BlueNoise = g_Texture2DTable[g_RenderPassData.BlueNoise];
		float4 blueNoise = BlueNoise[int2(launchIndex.x & 127, launchIndex.y & 127)];
		
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
			
			float3 ex = points[1] - points[0];
			float3 ey = points[3] - points[0];
			SphericalRectangle squad = SphericalRectangleInit(points[0], ex, ey, position);
			
			float diffProb, brdfProb;
			shadingResult.AnalyticUnshadowed += (Light.Color * Light.Luminance) * (ShadeWithAreaLight(position, diffuse, specular, ltcTerms, points, diffProb, brdfProb));
		
			static const uint RaysPerLight = 1;
			[unroll]
			for (uint rayIdx = 0; rayIdx < RaysPerLight; ++rayIdx)
			{
				uint I = i * RaysPerLight + rayIdx;
				// Used for 2D position on light/BRDF
				const float u1 = frac(Random(I) + blueNoise.r);
				const float u2 = frac(Random(I + 1) + blueNoise.g);
			
				// Choosing BRDF lobes
				const float u3 = frac(Random(I + 2) + blueNoise.b);
			
				// Choosing between light and BRDF sampling
				const float u4 = frac(Random(I + 3) + blueNoise.a);
		
				// BRDF sample
				if (u4 <= brdfProb)
				{
					// Randomly pick specular or diffuse lobe based on diffuse probability
					const bool bSpec = u3 > diffProb;
				
					// Importance sample the BRDF
					float3 w_i = BRDFSample(bSpec, u1, u2, ltcTerms);
					Ray ray = InitRay(position, 0.0f, normalize(w_i), FLT_MAX);
					Plane plane = InitPlane(Light.Orientation.xyz, Light.Position.xyz);
					float3 Y;
					if (RayPlaneIntersection(ray, plane, Y))
					{
						float3 delta = Y - position;
						float len = length(delta);
					
						float lightPDF = 1.0f / squad.SolidAngle;
					
						float brdfPDF;
						float3 f = BRDFEval(diffuse, specular, ltcTerms, w_i, diffProb, brdfPDF);
					
						const float misPDF = lerp(lightPDF, brdfPDF, brdfProb);
					
						// This test should almost never fail because we just importance sampled from the BRDF.
						// Only an all black BRDF or a precision issue could trigger this.
						if (brdfPDF > 0.0f)
						{
							// Stochastic estimate of incident light, without shadow
							// Note: projection cosine dot(w_i, n) is baked into the LTC-based BRDF
							float3 L_i = (Light.Color * Light.Luminance) * f / misPDF;
							if (any(isnan(L_i)))
							{
								L_i = 0.0.xxx;
							}
						
							shadingResult.StochasticUnshadowed += L_i;

							bool veryClose = (len < epsilon);
							if (!veryClose)
							{
								// Cast the shadow ray from the source towards the surface 
								// so that shadowing is consistent with front faces from the 
								// light's perspective. There's no reason to cast from the light
								// for coherence because this is an area source.
								float Visibility = TraceShadowRay(Y, epsilon, -w_i, max(len - epsilon, epsilon));
								shadingResult.StochasticShadowed += L_i * Visibility;
							}
						}
					}
				}
				// Light sample
				else
				{
					// Pick a random point on the light
					float3 Y = SphericalRectangleSample(squad, u1, u2);

					float3 delta = Y - position;
					float len = length(delta);
					float3 w_i = delta / max(len, 0.001f);

					float lightPDF = 1.0f / squad.SolidAngle;

					float brdfPDF;
					float3 f = BRDFEval(diffuse, specular, ltcTerms, w_i, diffProb, brdfPDF);

					const float misPDF = lerp(lightPDF, brdfPDF, brdfProb);

					if (brdfPDF > 0.0f)
					{
						// Stochastic estimate of incident light, without shadow
						// Note: projection cosine dot(w_i, n) is baked into the LTC-based BRDF
						float3 L_i = (Light.Color * Light.Luminance) * f / misPDF;
						if (any(isnan(L_i)))
						{
							L_i = 0.0.xxx;
						}

						shadingResult.StochasticUnshadowed += L_i;

						const bool veryClose = (len < epsilon);
						if (!veryClose)
						{
							// Cast the shadow ray from the source towards the surface 
							// so that shadowing is consistent with front faces from the 
							// light's perspective. There's no reason to cast from the light
							// for coherence because this is an area source.
							float Visibility = TraceShadowRay(Y, epsilon, -w_i, max(len - epsilon, epsilon));
							shadingResult.StochasticShadowed += L_i * Visibility;
						}
					}
				}
			}
		}
	}
	else if (GBufferType == GBufferTypeLight)
	{
		GBufferLight GBufferLight = GetGBufferLight(GBuffer, launchIndex);
		PolygonalLight Light = Lights[GBufferLight.LightIndex];
		shadingResult.AnalyticUnshadowed = (Light.Color * Light.Luminance);
	}
	
	shadingResult.AnalyticUnshadowed = ExposeLuminance(shadingResult.AnalyticUnshadowed, g_SystemConstants.Camera);
	shadingResult.StochasticUnshadowed = ExposeLuminance(shadingResult.StochasticUnshadowed, g_SystemConstants.Camera);
	shadingResult.StochasticShadowed = ExposeLuminance(shadingResult.StochasticShadowed, g_SystemConstants.Camera);
	
	RWTexture2D<float4> AnalyticUnshadowed = g_RWTexture2DTable[g_RenderPassData.AnalyticUnshadowed];
	RWTexture2D<float4> StochasticUnshadowed = g_RWTexture2DTable[g_RenderPassData.StochasticUnshadowed];
	RWTexture2D<float4> StochasticShadowed = g_RWTexture2DTable[g_RenderPassData.StochasticShadowed];
	
	AnalyticUnshadowed[launchIndex] = float4(shadingResult.AnalyticUnshadowed, 1.0f);
	StochasticUnshadowed[launchIndex] = float4(shadingResult.StochasticUnshadowed, 1.0f);
	StochasticShadowed[launchIndex] = float4(shadingResult.StochasticShadowed, 1.0f);
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