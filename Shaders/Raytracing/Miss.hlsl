#include "Common.hlsl"

// Miss.hlsl defines the Miss() shader, with its semantic [shader(�miss�)]. 
// This shader will be executed when no geometry is hit, and will write a constant color in the payload. 
// Note that this shader takes the payload as a inout parameter. It will be provided to the shader automatically by DXR.
[shader("miss")]
void Miss(inout RayPayload payload : SV_RayPayload)
{
    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);

    float ramp = launchIndex.y / dims.y;
    payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}