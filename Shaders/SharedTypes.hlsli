#ifndef SHARED_TYPES_HLSLI
#define SHARED_TYPES_HLSLI

// ==================== Material ====================
enum TextureTypes
{
	AlbedoIdx,
	NormalIdx,
	RoughnessIdx,
	MetallicIdx,
	EmissiveIdx,
	NumTextureTypes
};

#define TEXTURE_CHANNEL_RED		0
#define TEXTURE_CHANNEL_GREEN	1
#define TEXTURE_CHANNEL_BLUE	2
#define TEXTURE_CHANNEL_ALPHA	3

struct Material
{
	float3 baseColor;
	float metallic;
	float subsurface;
	float specular;
	float roughness;
	float specularTint;
	float anisotropic;
	float sheen;
	float sheenTint;
	float clearcoat;
	float clearcoatGloss;

	int TextureIndices[NumTextureTypes];
	int TextureChannel[NumTextureTypes];
};

// ==================== Light ====================
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
	squad.k = 2.0f * g_PI - g2 - g3;

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

#define LightType_Point (0)
#define LightType_Quad (1)

struct Light
{
	uint Type;
	float3 Position;
	float4 Orientation;
	float Width;
	float Height;
	float3 Points[4]; // World-space points that are pre-computed on the Cpu so we don't have to compute them in shader for every ray

	float3 I;
};

// ==================== Mesh ====================
struct Mesh
{
	uint		VertexOffset;
	uint		IndexOffset;
	uint		MaterialIndex;
	uint		InstanceIDAndMask;
	uint		InstanceContributionToHitGroupIndexAndFlags;
	uint64_t	AccelerationStructure;
	matrix		World;
	matrix		PreviousWorld;
	float3x4	Transform;
};

// ==================== Camera ====================
struct Camera
{
	float	NearZ;
	float	FarZ;
	float	ExposureValue100;
	float	_padding1;

	float	FocalLength;
	float	RelativeAperture;
	float	ShutterTime;
	float	SensorSensitivity;

	float4	Position;
	float4	U;
	float4	V;
	float4	W;

	matrix	View;
	matrix	Projection;
	matrix	ViewProjection;
	matrix	InvView;
	matrix	InvProjection;
	matrix	InvViewProjection;

	RayDesc GenerateCameraRay(in float2 ndc, inout uint seed)
	{
		float3 direction = ndc.x * U.xyz + ndc.y * V.xyz + W.xyz;

		// Find the focal point for this pixel
		direction /= length(W); // Make ray have length 1 along the camera's w-axis.
		float3 focalPoint = Position.xyz + FocalLength * direction; // Select point on ray a distance FocalLength along the w-axis

		// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
		float2 rnd = float2(g_2PI * RandomFloat01(seed), RelativeAperture * RandomFloat01(seed));
		float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

		// Use uv coordinate to compute a random origin on the camera lens
		float3 origin = Position.xyz + uv.x * normalize(U.xyz) + uv.y * normalize(V.xyz);
		direction = normalize(focalPoint - origin);

		RayDesc ray = { origin, NearZ, direction, FarZ };
		return ray;
	}
};

#endif // SHARED_TYPES_HLSLI