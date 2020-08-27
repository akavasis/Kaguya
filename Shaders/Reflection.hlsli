#ifndef __REFLECTION_HLSLI__
#define __REFLECTION_HLSLI__

// Reflection computations in here are the same in PBRT, they are evaluated in a reflection coordinate system
// where the 2 tangent vectors and the normal vector at the point being shaded are aligned with the x, y, z axes, respectively.

/* 
    Some important conventions and implementation details to keep in mind:
    1: The incident light direction wi and the outgoing viewing direction wo will be both
    normalized and outward facing after being transformed into the local coordinate system at the surface
    2: The surface normal will always points to the "outside" of the object, which makes it easy
    to determine if light is entering or exiting tranmissive objects: if the incident light direction wi
    is in the same hemisphere as n, then light is entering; otherwise, it is exiting. Therefore,
    one details to keep in mind is that the normal may be on the opposite side of the surface
    than one or both of the wi and wo directions.
    3: BRDF and BTDF implementations should not concern themselves with whether wi and wo lie in the same hemisphere.
    For example, although a reflective BRDF should in principle detect if the incident direction is
    above the surface and the outgoing direction is below and always return no reflection in this case,
    here we will expect the reflection function to instead compute and return the amount of light
    reflected using the appropriate formulas for their reflection model, ignoring the detail that
    they are not in the same hemisphere.
*/

float CosTheta(in float3 w)
{
    return w.z;
}
float Cos2Theta(in float3 w)
{
    return w.z * w.z;
}
float AbsCosTheta(in float3 w)
{
    return abs(w.z);
}
float Sin2Theta(in float3 w)
{
    return max(0.0f, 1.0f - Cos2Theta(w));
}
float SinTheta(in float3 w)
{
    return sqrt(Sin2Theta(w));
}
float TanTheta(in float3 w)
{
    return SinTheta(w) / CosTheta(w);
}
float Tan2Theta(in float3 w)
{
    return Sin2Theta(w) / Cos2Theta(w);
}

float CosPhi(in float3 w)
{
    float sinTheta = SinTheta(w);
    return sinTheta == 0.0f ? 1.0f : clamp(w.x / sinTheta, -1.0f, 1.0f);
}
float SinPhi(in float3 w)
{
    float sinTheta = SinTheta(w);
    return sinTheta == 0.0f ? 0.0f : clamp(w.y / sinTheta, -1.0, 1.0f);
}

float Cos2Phi(in float3 w)
{
    return CosPhi(w) * CosPhi(w);
}
float Sin2Phi(in float3 w)
{
    return SinPhi(w) * SinPhi(w);
}

#endif