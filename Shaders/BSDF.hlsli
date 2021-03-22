#ifndef BSDF_HLSLI
#define BSDF_HLSLI

#include <BxDF.hlsli>
#include <Disney.hlsli>
#include <SharedTypes.hlsli>

struct BSDF
{
	float3 WorldToLocal(float3 v)
	{
		return ShadingFrame.ToLocal(v);
	}
	
	float3 LocalToWorld(float3 v)
	{
		return ShadingFrame.ToWorld(v);
	}
	
	// Evaluate the BSDF for a pair of directions
	float3 f(float3 woW, float3 wiW)
	{
		float3 wo = WorldToLocal(woW), wi = WorldToLocal(wiW);
		if (wo.z == 0.0f)
		{
			return float3(0.0f, 0.0f, 0.0f);
		}
		
		return BxDF.f(wo, wi);
	}

	// Compute the pdf of sampling
	float Pdf(float3 woW, float3 wiW)
	{
		float3 wo = WorldToLocal(woW), wi = WorldToLocal(wiW);
		if (wo.z == 0.0f)
		{
			return 0.0f;
		}
		
		return BxDF.Pdf(wo, wi);
	}

	// Samples the BSDF
	bool Samplef(float3 woW, float2 Xi, out BSDFSample bsdfSample)
	{
		float3 wo = WorldToLocal(woW);
		if (wo.z == 0.0f)
		{
			return false;
		}
		
		bool success = BxDF.Samplef(woW, Xi, bsdfSample);
		if (!success || !any(bsdfSample.f) || bsdfSample.pdf == 0.0f || bsdfSample.wi.z == 0.0f)
		{
			return false;
		}
		
		bsdfSample.wi = LocalToWorld(bsdfSample.wi);
		return true;
	}
	
	float3 Ng;
	Frame ShadingFrame;
	
	Disney BxDF;
};

struct Interaction
{
	RayDesc SpawnRayTo(Interaction Interaction)
	{
		const float ShadowEpsilon = 0.0001f;
		
		float3 d = Interaction.p - p;
		float tmax = length(d);
		d = normalize(d);
		
		RayDesc ray = { p, 0.0001f, d, tmax - ShadowEpsilon };
		return ray;
	}
	
	float3 p; // Hit point
	float3 wo;
	float3 n; // Normal
};

struct SurfaceInteraction
{
	float3 p; // Hit point
	float3 wo;
	float3 n; // Normal
	float2 uv;
	Frame GeometryFrame;
	Frame ShadingFrame;
	BSDF BSDF;
	Material Material;
};

struct VisibilityTester
{
	Interaction I0;
	Interaction I1;
};

float3 SampleLi(Light light, SurfaceInteraction si, float2 Xi, out float3 pWi, out float pPdf, out VisibilityTester pVisibilityTester)
{
	if (light.Type == LightType_Point)
	{
		pWi = normalize(light.Position - si.p);
		pPdf = 1.0f;
	
		pVisibilityTester.I0.p = si.p;
		pVisibilityTester.I0.wo = si.wo;
		pVisibilityTester.I0.n = si.n;

		pVisibilityTester.I1.p = light.Position;
		pVisibilityTester.I1.wo = float3(0.0f, 0.0f, 0.0f);
		pVisibilityTester.I1.n = float3(0.0f, 0.0f, 0.0f);
	
		float d = length(light.Position - si.p);
		float d2 = d * d;
		return light.I / d2;
	}
	
	if (light.Type == LightType_Quad)
	{
		float3 ex = light.Points[1] - light.Points[0];
		float3 ey = light.Points[3] - light.Points[0];
		SphericalRectangle squad = SphericalRectangleInit(light.Points[0], ex, ey, si.p);
		
		// Pick a random point on the light
		float3 Y = SphericalRectangleSample(squad, Xi[0], Xi[1]);
		
		pWi = normalize(Y - si.p);
		pPdf = 1.0f / squad.SolidAngle;
		
		pVisibilityTester.I0.p = si.p;
		pVisibilityTester.I0.wo = si.wo;
		pVisibilityTester.I0.n = si.n;

		pVisibilityTester.I1.p = Y;
		pVisibilityTester.I1.wo = float3(0.0f, 0.0f, 0.0f);
		pVisibilityTester.I1.n = float3(0.0f, 0.0f, 0.0f);
		
		return light.I;
	}
	
	return 0.xxx;
}

#endif // BSDF_HLSLI