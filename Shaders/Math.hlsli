#ifndef MATH_HLSLI
#define MATH_HLSLI

static const float g_EPSILON = 1e-4f;

static const float g_PI = 3.141592654f;
static const float g_2PI = 6.283185307f;
static const float g_1DIVPI = 0.318309886f;
static const float g_1DIV2PI = 0.159154943f;
static const float g_1DIV4PI = 0.079577471f;
static const float g_PIDIV2 = 1.570796327f;
static const float g_PIDIV4 = 0.785398163f;

static const float FLT_MAX = asfloat(0x7F7FFFFF);

void CoordinateSystem(float3 v1, out float3 v2, out float3 v3)
{
	if (abs(v1.x) > abs(v1.y))
	{
		v2 = float3(-v1.z, 0.0f, v1.x) / sqrt(v1.x * v1.x + v1.z * v1.z);
	}
	else
	{
		v2 = float3(0.0f, v1.z, -v1.y) / sqrt(v1.y * v1.y + v1.z * v1.z);
	}
	v3 = cross(v1, v2);
}

float3 Faceforward(float3 n, float3 v)
{
    return (dot(n, v) < 0.f) ? -n : n;
}

struct Frame
{
	float3 ToWorld(float3 v)
	{
		return s * v.x + t * v.y + n * v.z;
	}

	float3 ToLocal(float3 v)
	{
		return float3(dot(v, s), dot(v, t), dot(v, n));
	}
	
	// tangent, bitangent, normal
	float3 s;
	float3 t;
	float3 n;
};

Frame InitFrame(float3 n)
{
	Frame frame;
	frame.n = n;
	
	CoordinateSystem(frame.n, frame.s, frame.t);
	
	return frame;
}

struct Ray
{
	float3 Origin;
	float TMin;
	float3 Direction;
	float TMax;
};

Ray InitRay(float3 origin, float tMin, float3 direction, float tMax)
{
	Ray ray;
	ray.Origin = origin;
	ray.TMin = tMin;
	ray.Direction = direction;
	ray.TMax = tMax;
	return ray;
}

Ray InitRay(float3 origin, float3 direction)
{
	return InitRay(origin, 0.0f, direction, FLT_MAX);
}

struct Plane
{
	float3 Normal;
	float3 PointOnPlane;
};

Plane InitPlane(float3 normal, float3 pointOnPlane)
{
	Plane plane;
	plane.Normal = normal;
	plane.PointOnPlane = pointOnPlane;
	return plane;
}

bool RayPlaneIntersection(Ray ray, Plane plane, out float t)
{
    // Assuming float3s are all normalized
	float denom = dot(plane.Normal, ray.Direction) + 1e-06;
	float3 p0l0 = plane.PointOnPlane - ray.Origin;
	t = dot(p0l0, plane.Normal) / denom;
	return t > 0.0f;
}

// Sampling the Solid Angle of Area Light Sources
// https://schuttejoe.github.io/post/arealightsampling/
// Solid-angle rectangular light sampling yields lower variance and faster convergence.
// An Area-Preserving Parametrization for Spherical Rectangles:
// https://www.arnoldrenderer.com/research/egsr2013_spherical_rectangle.pdf
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

#endif // MATH_HLSLI