#include "StaticSamplers.hlsli"
#include "DescriptorTables.hlsli"

#include "HLSLCommon.hlsli"
#include "PBR.hlsli"

ConstantBuffer<ObjectConstants> ObjectConstantsGPU : register(b0);
ConstantBuffer<RenderPassConstants> RenderPassConstantsGPU : register(b1);

StructuredBuffer<MaterialTextureIndices> MaterialIndices : register(t0, space100);
StructuredBuffer<MaterialTextureProperties> MaterialProperties : register(t0, space101);

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
	
	MaterialTextureIndices materialIndices = MaterialIndices[ObjectConstantsGPU.MaterialIndex];
	MaterialTextureProperties materialProperties = MaterialProperties[ObjectConstantsGPU.MaterialIndex];
	
	// Fetch albedo/diffuse data
	float alpha = 1.0f;
	if (materialIndices.AlbedoMapIndex != -1)
	{
		Texture2D albedoMap = Tex2DTable[materialIndices.AlbedoMapIndex];
		float4 albedoMapSample = albedoMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
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
		float4 normalMapSample = normalMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		float3 normalT = normalize(2.0f * normalMapSample.rgb - 1.0f); // Uncompress each component from [0,1] to [-1,1].
		inputPixel.normalW = normalize(mul(normalT, inputPixel.tbnMatrix)); // Transform from tangent space to world space.
	}
	
	// Fetch roughness data
	if (materialIndices.RoughnessMapIndex != -1)
	{
		Texture2D roughnessMap = Tex2DTable[materialIndices.RoughnessMapIndex];
		float4 roughnessMapSample = roughnessMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Roughness = roughnessMapSample.r;
	}
	
	// Fetch metallic data
	if (materialIndices.MetallicMapIndex != -1)
	{
		Texture2D metallicMap = Tex2DTable[materialIndices.MetallicMapIndex];
		float4 metallicMapSample = metallicMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Metallic = metallicMapSample.r;
	}
	
	// Fetch emissive data
	if (materialIndices.EmissiveMapIndex != -1)
	{
		Texture2D emissiveMap = Tex2DTable[materialIndices.EmissiveMapIndex];
		float4 emissiveMapSample = emissiveMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		materialProperties.Emissive = emissiveMapSample.rgb;
	}

    // Vector from point being lit to eye. 
	float3 viewDirection = normalize(RenderPassConstantsGPU.EyePosition - inputPixel.positionW);
	
	float3 color = materialProperties.Emissive;
	
	// Shadow
	Texture2DArray sunShadowMap = Tex2DArrayTable[RenderPassConstantsGPU.SunShadowMapIndex];
	int cascadeIndex = 0;
	for (int i = NUM_CASCADES - 1; i >= 0; --i)
	{
		if (inputPixel.depth <= RenderPassConstantsGPU.Sun.Cascades[i].Split)
		{
			cascadeIndex = i;
		}
	}
	float shadowFactor = CalcShadowRatio(inputPixel.positionW, cascadeIndex, RenderPassConstantsGPU.Sun.Cascades, s_SamplerShadow, sunShadowMap);
	float3 radiance = RenderPassConstantsGPU.Sun.Strength * RenderPassConstantsGPU.Sun.Intensity * shadowFactor;
	color += PBR(materialProperties, inputPixel.normalW, viewDirection, -RenderPassConstantsGPU.Sun.Direction, radiance);
	
	// Ambient lighting (IBL)
	{
		Texture2D brdfLUTMap = Tex2DTable[RenderPassConstantsGPU.BRDFLUTMapIndex];
		TextureCube irradianceCubemap = TexCubeTable[RenderPassConstantsGPU.IrradianceCubemapIndex];
		TextureCube prefilteredCubemap = TexCubeTable[RenderPassConstantsGPU.PrefilteredRadianceCubemapIndex];
		
		float cosLo = max(0.0f, dot(inputPixel.normalW, viewDirection));
		
		// Specular reflection vector
		float3 irradiance = irradianceCubemap.Sample(s_SamplerAnisotropicWrap, inputPixel.normalW).rgb;
		
		// Sample pre-filtered specular reflection environment at correct mipmap level
		float3 Lr = reflect(-viewDirection, inputPixel.normalW);
		uint width, height, mipLevel;
		prefilteredCubemap.GetDimensions(0, width, height, mipLevel);
		float3 prefiltered = prefilteredCubemap.SampleLevel(s_SamplerAnisotropicWrap, Lr, materialProperties.Roughness * mipLevel).rgb;
		
		// Split-sum approximation factors for Cook-Torrance specular BRDF
		float2 brdfIntegration = brdfLUTMap.Sample(s_SamplerLinearClamp, float2(cosLo, materialProperties.Roughness)).rg;
		
		color += IBL(materialProperties, cosLo, irradiance, prefiltered, brdfIntegration);
	}
	
	return float4(color, alpha);
}