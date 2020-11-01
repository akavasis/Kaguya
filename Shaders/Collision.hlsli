#ifndef COLLISION_HLSLI
#define COLLISION_HLSLI

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
	return InitRay(origin, 0.0, direction, 3.402823466e+38);
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

#endif