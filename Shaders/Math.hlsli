#ifndef MATH_HLSLI
#define MATH_HLSLI

static const float s_PI = 3.141592654f;
static const float s_2PI = 6.283185307f;
static const float s_1DIVPI = 0.318309886f;
static const float s_1DIV2PI = 0.159154943f;
static const float s_1DIV4PI = 0.079577471f;
static const float s_PIDIV2 = 1.570796327f;
static const float s_PIDIV4 = 0.785398163f;

static const float FLT_MAX = asfloat(0x7F7FFFFF);

struct Ray
{
	float3	Origin;
	float	TMin;
	float3	Direction;
	float	TMax;
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
	float3	Normal;
	float3	PointOnPlane;
};

Plane InitPlane(float3 normal, float3 pointOnPlane)
{
	Plane plane;
	plane.Normal = normal;
	plane.PointOnPlane = pointOnPlane;
	return plane;
}

bool RayPlaneIntersection(Ray ray, Plane plane, out float3 intersectionPoint)
{
    // Assuming float3s are all normalized
	float denom = dot(plane.Normal, ray.Direction) + 1e-06;
	float3 p0l0 = plane.PointOnPlane - ray.Origin;
	float t = dot(p0l0, plane.Normal) / denom;
	intersectionPoint = ray.Origin + ray.Direction * t;
	return t >= 0;
}

#endif // MATH_HLSLI