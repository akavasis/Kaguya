#include "HLSLCommon.hlsli"
#include "PBR.hlsli"

StructuredBuffer<MaterialTextureIndices> MaterialIndices : register(t0, space0);
StructuredBuffer<MaterialTextureProperties> MaterialProperties : register(t0, space1);

// Shader layout define and include
#define ConstantDataType ObjectConstants
#define RenderPassDataType RenderPassConstants
#include "ShaderLayout.hlsli"

struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 positionW : POSITION;
	float2 textureCoord : TEXCOORD;
	float3 normalW : NORMAL;
	float3x3 tbnMatrix : TBNBASIS;
	float depth : DEPTH;
};
float4 main(OutputVertex inputPixel) : SV_TARGET
{
	inputPixel.normalW = normalize(inputPixel.normalW);
	
	MaterialTextureIndices materialIndices = MaterialIndices[ConstantDataCB.MaterialIndex];
	MaterialTextureProperties materialProperties = MaterialProperties[ConstantDataCB.MaterialIndex];
	
	// Fetch albedo/diffuse data
	float alpha = 1.0f;
	if (materialIndices.AlbedoMapIndex != -1)
	{
		Texture2D albedoMap = Tex2DTable[materialIndices.AlbedoMapIndex];
		float4 albedoMapSample = albedoMap.Sample(SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Albedo = albedoMapSample.rgb;
		alpha = albedoMapSample.a;
		if (materialIndices.IsMasked)
		{
			clip(alpha < 0.1f ? -1.0f : 1.0f);
		}
	}
	
	// Fetch normal data
	if (materialIndices.NormalMapIndex != -1)
	{
		Texture2D normalMap = Tex2DTable[materialIndices.NormalMapIndex];
		float4 normalMapSample = normalMap.Sample(SamplerAnisotropicWrap, inputPixel.textureCoord);
		float3 normalT = normalize(2.0f * normalMapSample.rgb - 1.0f); // Uncompress each component from [0,1] to [-1,1].
		inputPixel.normalW = normalize(mul(normalT, inputPixel.tbnMatrix)); // Transform from tangent space to world space.
	}
	
	// Fetch roughness data
	if (materialIndices.RoughnessMapIndex != -1)
	{
		Texture2D roughnessMap = Tex2DTable[materialIndices.RoughnessMapIndex];
		float4 roughnessMapSample = roughnessMap.Sample(SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Roughness = roughnessMapSample.r;
	}
	
	// Fetch metallic data
	if (materialIndices.MetallicMapIndex != -1)
	{
		Texture2D metallicMap = Tex2DTable[materialIndices.MetallicMapIndex];
		float4 metallicMapSample = metallicMap.Sample(SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Metallic = metallicMapSample.r;
	}
	
	// Fetch emissive data
	if (materialIndices.EmissiveMapIndex != -1)
	{
		Texture2D emissiveMap = Tex2DTable[materialIndices.EmissiveMapIndex];
		float4 emissiveMapSample = emissiveMap.Sample(SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Emissive = emissiveMapSample.rgb;
	}

    // Vector from point being lit to eye. 
	float3 viewDirection = normalize(RenderPassDataCB.EyePosition - inputPixel.positionW);
	
	float3 color = materialProperties.Emissive;
	
	// Shadow
	Texture2DArray sunShadowMap = Tex2DArrayTable[RenderPassDataCB.SunShadowMapIndex];
	int cascadeIndex = 0;
	for (int i = NUM_CASCADES - 1; i >= 0; --i)
	{
		if (inputPixel.depth <= RenderPassDataCB.Sun.Cascades[i].Split)
		{
			cascadeIndex = i;
		}
	}
	float shadowFactor = CalcShadowRatio(inputPixel.positionW, cascadeIndex, RenderPassDataCB.Sun.Cascades, SamplerShadow, sunShadowMap);
	float3 radiance = RenderPassDataCB.Sun.Strength * RenderPassDataCB.Sun.Intensity * shadowFactor;
	color += PBR(materialProperties, inputPixel.normalW, viewDirection, -RenderPassDataCB.Sun.Direction, radiance);
	
	// Ambient lighting (IBL)
	{
		Texture2D brdfLUTMap = Tex2DTable[RenderPassDataCB.BRDFLUTMapIndex];
		TextureCube irradianceCubemap = TexCubeTable[RenderPassDataCB.IrradianceCubemapIndex];
		TextureCube prefilteredCubemap = TexCubeTable[RenderPassDataCB.PrefilteredRadianceCubemapIndex];
		
		float cosLo = max(0.0f, dot(inputPixel.normalW, viewDirection));
		
		// Specular reflection vector
		float3 irradiance = irradianceCubemap.Sample(SamplerAnisotropicWrap, inputPixel.normalW).rgb;
		
		// Sample pre-filtered specular reflection environment at correct mipmap level
		float3 Lr = reflect(-viewDirection, inputPixel.normalW);
		uint width, height, mipLevel;
		prefilteredCubemap.GetDimensions(0, width, height, mipLevel);
		float3 prefiltered = prefilteredCubemap.SampleLevel(SamplerAnisotropicWrap, Lr, materialProperties.Roughness * mipLevel).rgb;
		
		// Split-sum approximation factors for Cook-Torrance specular BRDF
		float2 brdfIntegration = brdfLUTMap.Sample(SamplerLinearClamp, float2(cosLo, materialProperties.Roughness)).rg;
		
		color += IBL(materialProperties, cosLo, irradiance, prefiltered, brdfIntegration);
	}
	
	return float4(color, alpha);
}