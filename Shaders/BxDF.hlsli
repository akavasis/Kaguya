#ifndef BXDF_HLSLI
#define BXDF_HLSLI

#include <Math.hlsli>
#include <Sampling.hlsli>

inline float CosTheta(float3 w)
{
	return w.z;
}
inline float Cos2Theta(float3 w)
{
	return w.z * w.z;
}
inline float AbsCosTheta(float3 w)
{
	return abs(w.z);
}
inline float Sin2Theta(float3 w)
{
	return max(0.0f, 1.0f - Cos2Theta(w));
}
inline float SinTheta(float3 w)
{
	return sqrt(Sin2Theta(w));
}
inline float TanTheta(float3 w)
{
	return SinTheta(w) / CosTheta(w);
}
inline float Tan2Theta(float3 w)
{
	return Sin2Theta(w) / Cos2Theta(w);
}
inline float CosPhi(float3 w)
{
	float sinTheta = SinTheta(w);
	return (sinTheta == 0.0f) ? 1.0f : clamp(w.x / sinTheta, -1.0f, 1.0f);
}
inline float SinPhi(float3 w)
{
	float sinTheta = SinTheta(w);
	return (sinTheta == 0.0f) ? 0.0f : clamp(w.y / sinTheta, -1.0f, 1.0f);
}
inline float Cos2Phi(float3 w)
{
	return CosPhi(w) * CosPhi(w);
}
inline float Sin2Phi(float3 w)
{
	return SinPhi(w) * SinPhi(w);
}

inline bool SameHemisphere(float3 v0, float3 v1)
{
	return v0.z * v1.z > 0;
}

inline float3 Reflect(float3 wo, float3 n)
{
	return -wo + 2.0f * dot(wo, n) * n;
}

inline bool Refract(float3 wi, float3 n, float eta, out float3 wt)
{
	// Compute $\cos \theta_\roman{t}$ using Snell's law
	float cosThetaI = dot(n, wi);
	float sin2ThetaI = max(0.0f, float(1.0f - cosThetaI * cosThetaI));
	float sin2ThetaT = eta * eta * sin2ThetaI;

	// Handle total internal reflection for transmission
	if (sin2ThetaT >= 1)
		return false;
	float cosThetaT = sqrt(1 - sin2ThetaT);
	wt = eta * -wi + (eta * cosThetaI - cosThetaT) * n;
	return true;
}

#define BxDFTypes_Unknown (0)
#define BxDFTypes_Reflection (1 << 0)
#define BxDFTypes_Transmission (1 << 1)
#define BxDFTypes_All (BxDFTypes_Reflection | BxDFTypes_Transmission)

enum BxDFFlags
{
	Unknown = 0,
	Reflection = 1 << 0,
	Transmission = 1 << 1,

	Diffuse = 1 << 2,
	Glossy = 1 << 3,
	Specular = 1 << 4,
	// Composite flags definitions
	DiffuseReflection = Diffuse | Reflection,
	DiffuseTransmission = Diffuse | Transmission,
	GlossyReflection = Glossy | Reflection,
	GlossyTransmission = Glossy | Transmission,
	SpecularReflection = Specular | Reflection,
	SpecularTransmission = Specular | Transmission,

	All = Diffuse | Glossy | Specular | Reflection | Transmission
};

// BxDFFlags Inline Functions
inline bool IsReflective(BxDFFlags f)
{
	return f & BxDFFlags::Reflection;
}
inline bool IsTransmissive(BxDFFlags f)
{
	return f & BxDFFlags::Transmission;
}
inline bool IsDiffuse(BxDFFlags f)
{
	return f & BxDFFlags::Diffuse;
}
inline bool IsGlossy(BxDFFlags f)
{
	return f & BxDFFlags::Glossy;
}
inline bool IsSpecular(BxDFFlags f)
{
	return f & BxDFFlags::Specular;
}

struct BSDFSample
{
	float3 f;
	float3 wi;
	float pdf;
	BxDFFlags flags;
};

BSDFSample InitBSDFSample(float3 f, float3 wi, float pdf, BxDFFlags flags)
{
	BSDFSample bsdfSample;
	bsdfSample.f = f;
	bsdfSample.wi = wi;
	bsdfSample.pdf = pdf;
	bsdfSample.flags = flags;
	
	return bsdfSample;
}

struct LambertianReflection
{
	float3 f(float3 wo, float3 wi)
	{
		if (!SameHemisphere(wo, wi))
		{
			return float3(0.0f, 0.0f, 0.0f);
		}
		
		return R * g_1DIVPI;
	}
	
	float Pdf(float3 wo, float3 wi)
	{
		if (!SameHemisphere(wo, wi))
		{
			return 0.0f;
		}
		
		return CosineHemispherePdf(AbsCosTheta(wi));
	}
	
	bool Samplef(float3 wo, float2 Xi, out BSDFSample bsdfSample)
	{
		float3 wi = SampleCosineHemisphere(Xi);
		if (wo.z < 0.0f)
		{
			wi.z *= -1.0f;
		}
		
		bsdfSample = InitBSDFSample(f(wo, wi), wi, Pdf(wo, wi), Flags());
		
		return true;
	}
	
	BxDFFlags Flags()
	{
		return BxDFFlags::DiffuseReflection;
	}
	
	float3 R;
};

#endif // BXDF_HLSLI