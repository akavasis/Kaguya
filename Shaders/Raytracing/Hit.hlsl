#include "Common.hlsl"

// It will be executed upon hitting the geometry (our triangle). 
// As the miss shader, it takes the ray payload payload as a inout parameter. 
// It also has a second parameter defining the intersection attributes as provided by the intersection shader, 
// ie. the barycentric coordinates. This shader simply writes a constant color to the payload, as well as the distance from the origin, 
// provided by the built-in RayCurrentT() function.
[shader("closesthit")] 
void ClosestHit(inout RayPayload payload, Attributes attrib) 
{
    float3 barycentrics =
    float3(1.f - attrib.bary.x - attrib.bary.y, attrib.bary.x, attrib.bary.y);

    const float3 A = float3(1, 0, 0);
    const float3 B = float3(0, 1, 0);
    const float3 C = float3(0, 0, 1);

    float3 hitColor = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;

    payload.colorAndDistance = float4(hitColor, RayTCurrent());
}