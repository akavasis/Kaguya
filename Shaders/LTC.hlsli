#ifndef LTC_HLSLI
#define LTC_HLSLI

#include "HLSLCommon.hlsli"

static const float LUT_SIZE = 64.0f;
static const float LUT_SCALE = (LUT_SIZE - 1.0f) / LUT_SIZE;
static const float LUT_BIAS = 0.5f / LUT_SIZE;

struct LTCTerms
{
	float3x3	MinvS;
	float3x3	MS;
	float		MdetS;

	float3x3	MinvD;
	float3x3	MD;
	float		MdetD;
	
	float		Magnitude;
	float		Fresnel;
};

/* Get uv coordinates into LTC lookup texture */
float2 GetLTC_LUT_UV(float NoV, float roughness)
{
	float2 uv = float2(roughness, sqrt(1.0f - NoV));
	uv = uv * LUT_SCALE + LUT_BIAS;
	return uv;
}

/* Get inverse matrix from LTC lookup texture */
float3x3 GetLTC_LUT_InverseMatrix(Texture2D Texture, SamplerState Sampler, float2 UV)
{
	float4 v = Texture.SampleLevel(Sampler, UV, 0.0f);
	float3x3 invM = float3x3(
	   float3(v.x,   0, v.y),
	   float3(  0,   1,   0),
	   float3(v.z,   0, v.w)
	);
	return invM;
}

/* Get terms from LTC lookup texture */
float4 GetLTC_LUT_Terms(Texture2D Texture, SamplerState Sampler, float2 UV)
{
	return Texture.SampleLevel(Sampler, UV, 0.0f);
}

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
	if (L[0].z >= 0.0f) config += 1;
	if (L[1].z >= 0.0f) config += 2;
	if (L[2].z >= 0.0f) config += 4;
	if (L[3].z >= 0.0f) config += 8;

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

float LTC_Evaluate(float3 P, float3x3 Minv, float3 points[4])
{
	// MInv matrix is expected to be multiplied by a World to Tangent space matrix 
	// Rotate area light in (T1, T2, N) basis
	//float3 T1, T2;
	//T1 = normalize(V - N * dot(V, N));
	//T2 = cross(N, T1);
	//float3x3 Rinv = float3x3(T1, T2, N);
	//float3x3 R = transpose(Rinv);
	//Minv = mul(R, Minv);

    // polygon (allocate 5 vertices for clipping)
	float3 L[5];
	L[0] = mul((points[0] - P), Minv);
	L[1] = mul((points[1] - P), Minv);
	L[2] = mul((points[2] - P), Minv);
	L[3] = mul((points[3] - P), Minv);

	int n;
	ClipQuadToHorizon(L, n);

	if (n == 0)
		return 0.0f;

	// Project onto sphere
	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	// Integrate
	float Lo_i = 0.0f;
	Lo_i += IntegrateEdge(L[0], L[1]);
	Lo_i += IntegrateEdge(L[1], L[2]);
	Lo_i += IntegrateEdge(L[2], L[3]);

	if (n >= 4)
		Lo_i += IntegrateEdge(L[3], L[4]);

	if (n == 5)
		Lo_i += IntegrateEdge(L[4], L[0]);

	return max(0.0f, Lo_i);
}

/* L = vector to the light from the surface */
float LTC_Sample_PDF(float3x3 MInv, float MDet, float3 L)
{
	float3 LCosine = mul(L, MInv);
	float l2 = dot(LCosine, LCosine);
	float Jacobian = MDet * l2 * l2;

	return max(LCosine.z, 0.0f) / (s_PI * Jacobian);
}

float3 LTC_Sample_Vector(float3x3 M, float u1, float u2)
{
	float ct = sqrt(u1);
	float st = sqrt(1.0f - u1);
	float phi = 2.0f * s_PI * u2;

	float3 dir = float3(st * cos(phi), st * sin(phi), ct);

	float3 L = mul(dir, M);
	float3 w_i = normalize(L);

	return w_i;
}

struct SphericalRectangle
{
	float3 o, x, y, z;
	float z0, z0sq;
	float x0, y0, y0sq;
	float x1, y1, y1sq;
	float b0, b1, b0sq, k;
	float SolidAngle;
};

SphericalRectangle SphericalRectangleInit(float3 s, float3 ex, float3 ey, float3 o)
{
	SphericalRectangle squad;

	squad.o = o;
	float exl = length(ex);
	float eyl = length(ey);

    // compute local reference system 'R'
	squad.x = ex / exl;
	squad.y = ey / eyl;
	squad.z = cross(squad.x, squad.y);

    // compute rectangle coords in local reference system
	float3 d = s - o;
	squad.z0 = dot(d, squad.z);

    // flip 'z' to make it point against 'Q'
	if (squad.z0 > 0.0f)
	{
		squad.z = -squad.z;
		squad.z0 = -squad.z0;
	}

	squad.z0sq = squad.z0 * squad.z0;
	squad.x0 = dot(d, squad.x);
	squad.y0 = dot(d, squad.y);
	squad.x1 = squad.x0 + exl;
	squad.y1 = squad.y0 + eyl;
	squad.y0sq = squad.y0 * squad.y0;
	squad.y1sq = squad.y1 * squad.y1;

    // create vectors to four vertices
	float3 v00 = float3(squad.x0, squad.y0, squad.z0);
	float3 v01 = float3(squad.x0, squad.y1, squad.z0);
	float3 v10 = float3(squad.x1, squad.y0, squad.z0);
	float3 v11 = float3(squad.x1, squad.y1, squad.z0);

    // compute normals to edges
	float3 n0 = normalize(cross(v00, v10));
	float3 n1 = normalize(cross(v10, v11));
	float3 n2 = normalize(cross(v11, v01));
	float3 n3 = normalize(cross(v01, v00));

    // compute internal angles (gamma_i)
	float g0 = acos(-dot(n0, n1));
	float g1 = acos(-dot(n1, n2));
	float g2 = acos(-dot(n2, n3));
	float g3 = acos(-dot(n3, n0));

    // compute predefined constants
	squad.b0 = n0.z;
	squad.b1 = n2.z;
	squad.b0sq = squad.b0 * squad.b0;
	squad.k = 2.0f * s_PI - g2 - g3;

    // compute solid angle from internal angles
	squad.SolidAngle = g0 + g1 - squad.k;

	return squad;
}

float3 SphericalRectangleSample(SphericalRectangle squad, float u, float v)
{
    // 1. compute 'cu'
	float au = u * squad.SolidAngle + squad.k;
	float fu = (cos(au) * squad.b0 - squad.b1) / sin(au);
	float cu = 1.0f / sqrt(fu * fu + squad.b0sq) * (fu > 0.0f ? 1.0f : -1.0f);
	cu = clamp(cu, -1.0f, 1.0f); // avoid NaNs

    // 2. compute 'xu'
	float xu = -(cu * squad.z0) / sqrt(1.0f - cu * cu);
	xu = clamp(xu, squad.x0, squad.x1); // avoid Infs

    // 3. compute 'yv'
	float d = sqrt(xu * xu + squad.z0sq);
	float h0 = squad.y0 / sqrt(d * d + squad.y0sq);
	float h1 = squad.y1 / sqrt(d * d + squad.y1sq);
	float hv = h0 + v * (h1 - h0), hv2 = hv * hv;
	float yv = (hv2 < 1.0f - 1e-6f) ? (hv * d) / sqrt(1.0f - hv2) : squad.y1;

    // 4. transform (xu, yv, z0) to world coords
	return squad.o + xu * squad.x + yv * squad.y + squad.z0 * squad.z;
}

float3 SphericalRectangleSampleSurfaceToLightVector(SphericalRectangle squad, float u, float v)
{
	float3 pointOnLight = SphericalRectangleSample(squad, u, v);
	return normalize(pointOnLight - squad.o);
}

#endif