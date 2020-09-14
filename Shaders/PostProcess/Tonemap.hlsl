struct TonemapData
{
	float Exposure;
	float Gamma;
	unsigned int InputMapIndex;
};

// Shader layout define and include
#define ConstantDataType TonemapData
#include "../ShaderLayout.hlsli"

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

float3 LessThan(float3 f, float value)
{
	return float3(
        (f.x < value) ? 1.0f : 0.0f,
        (f.y < value) ? 1.0f : 0.0f,
        (f.z < value) ? 1.0f : 0.0f);
}

float3 LinearToSRGB(float3 rgb)
{
	rgb = clamp(rgb, 0.0f, 1.0f);
     
	return lerp(
        pow(rgb, float3(1.0f / 2.4f, 1.0f / 2.4f, 1.0f / 2.4f)) * 1.055f - 0.055f,
        rgb * 12.92f,
        LessThan(rgb, 0.0031308f)
    );
}
 
float3 SRGBToLinear(float3 rgb)
{
	rgb = clamp(rgb, 0.0f, 1.0f);
     
	return lerp(
        pow(((rgb + 0.055f) / 1.055f), float3(2.4f, 2.4f, 2.4f)),
        rgb / 12.92f,
        LessThan(rgb, 0.04045f)
    );
}

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};
float4 main(VSInput pixel) : SV_TARGET
{
	float3 color = Tex2DTable[ConstantDataCB.InputMapIndex].SampleLevel(SamplerLinearClamp, pixel.Texture, 0.0f).rgb;
	
	// apply exposure (how long the shutter is open)
	color *= ConstantDataCB.Exposure;
	
	// convert unbounded HDR color range to SDR color range
	color = ACESFilm(color);
	
	// convert from linear to sRGB for display
	color = LinearToSRGB(color);
	return float4(color, 1.0f);
}