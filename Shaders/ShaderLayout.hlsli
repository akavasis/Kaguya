#ifndef __SHADER_LAYOUT_HLSLI__
#define __SHADER_LAYOUT_HLSLI__
struct DummyStructure
{
	int Dummy;
};

#ifndef ConstantDataType
#define ConstantDataType DummyStructure
#endif

#ifndef RenderPassDataType
#define RenderPassDataType DummyStructure
#endif

ConstantBuffer<ConstantDataType>	ConstantDataCB		: register(b0, space100);
ConstantBuffer<RenderPassDataType>	RenderPassDataCB	: register(b1, space100);

Texture2D				Tex2DTable[]					: register(t0, space100);
Texture2DArray			Tex2DArrayTable[]				: register(t0, space101);
TextureCube				TexCubeTable[]					: register(t0, space102);
ByteAddressBuffer		RawBufferTable[]				: register(t0, space103);

// Static Samplers
SamplerState			SamplerPointWrap				: register(s0);
SamplerState			SamplerPointClamp				: register(s1);
SamplerState			SamplerLinearWrap				: register(s2);
SamplerState			SamplerLinearClamp				: register(s3);
SamplerState			SamplerAnisotropicWrap			: register(s4);
SamplerState			SamplerAnisotropicClamp			: register(s5);
SamplerComparisonState	SamplerShadow					: register(s6);
#endif