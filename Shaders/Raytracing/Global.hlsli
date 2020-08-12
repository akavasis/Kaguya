// This file defines global root signature for raytracing shaders
#include "../HLSLCommon.hlsli"
#include "Common.hlsli"

RaytracingAccelerationStructure SceneBVH : register(t0, space0);
StructuredBuffer<Vertex> VertexBuffer : register(t1, space0);
StructuredBuffer<uint> IndexBuffer : register(t2, space0);
StructuredBuffer<GeometryInfo> GeometryInfoBuffer : register(t3, space0);

RWTexture2D<float4> RenderTarget : register(u0, space0);
ConstantBuffer<RenderPassConstants> RenderPassDataCB : register(b0, space0);