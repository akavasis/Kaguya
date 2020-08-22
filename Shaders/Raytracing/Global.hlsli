// This file defines global root signature for raytracing shaders
#include "../HLSLCommon.hlsli"
#include "../PBR.hlsli"
#include "Common.hlsli"

RaytracingAccelerationStructure SceneBVH                        : register(t0, space0);
StructuredBuffer<Vertex> VertexBuffer                           : register(t1, space0);
StructuredBuffer<uint> IndexBuffer                              : register(t2, space0);
StructuredBuffer<GeometryInfo> GeometryInfoBuffer               : register(t3, space0);
StructuredBuffer<MaterialTextureIndices> MaterialIndices        : register(t4, space0);
StructuredBuffer<MaterialTextureProperties> MaterialProperties  : register(t5, space0);

RWTexture2D<float4> RenderTarget                                : register(u0, space0);

#define RenderPassDataType RenderPassConstants
#include "../ShaderLayout.hlsli"