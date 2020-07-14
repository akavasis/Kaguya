#ifndef __STATIC_SAMPLERS_HLSLI__
#define __STATIC_SAMPLERS_HLSLI__
SamplerState s_SamplerPointWrap : register(s0);
SamplerState s_SamplerPointClamp : register(s1);
SamplerState s_SamplerLinearWrap : register(s2);
SamplerState s_SamplerLinearClamp : register(s3);
SamplerState s_SamplerAnisotropicWrap : register(s4);
SamplerState s_SamplerAnisotropicClamp : register(s5);
SamplerComparisonState s_SamplerShadow : register(s6);
#endif