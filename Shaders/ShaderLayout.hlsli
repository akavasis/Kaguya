#ifndef __SHADER_LAYOUT_HLSLI__
#define __SHADER_LAYOUT_HLSLI__

struct DummyStructure
{
	int Dummy;
};

#ifndef RenderPassDataType
#define RenderPassDataType DummyStructure
#endif

ConstantBuffer<RenderPassDataType>	RenderPassData				: register(b0, space100);

Texture2D					Texture2DTable[]					: register(t0, space100);
Texture2DArray				Texture2DArrayTable[]				: register(t0, space101);
TextureCube					TextureCubeTable[]					: register(t0, space102);
ByteAddressBuffer			RawBufferTable[]					: register(t0, space103);

RWTexture2D<float4>			RWTexture2DTable[]					: register(u0, space100);
RWTexture2DArray<float4>	RWTexture2DArrayTable[]				: register(u0, space101);

SamplerState				SamplerTable[]						: register(s0, space100);

#endif