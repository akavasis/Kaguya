#include "HLSLCommon.hlsli"
#include "StaticSamplers.hlsli"
#include "PBR.hlsli"

ConstantBuffer<Object> ObjectConstantsGPU : register(b0);
ConstantBuffer<Material> MaterialConstantsGPU : register(b1);
ConstantBuffer<RenderPass> RenderPassConstantsGPU : register(b2);

Texture2D AlbedoMap : register(t0);
Texture2D NormalMap : register(t1);
Texture2D RoughnessMap : register(t2);
Texture2D MetallicMap : register(t3);
Texture2D EmissiveMap : register(t4);

Texture2DArray ShadowMap : register(t5);
TextureCube IrradianceMap : register(t6);
TextureCube PrefilteredRadianceMap : register(t7);
Texture2D BRDFLUT : register(t8);

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
	
	MaterialSurfacePBR surface;
	surface.Albedo = MaterialConstantsGPU.Albedo;
	surface.Metallic = MaterialConstantsGPU.Metallic;
	surface.Emissive = MaterialConstantsGPU.Emissive;
	surface.Roughness = MaterialConstantsGPU.Roughness;
	
	float alpha = 1.0f;
	if (HaveAlbedoMap(MaterialConstantsGPU.Flags))
	{
		float4 albedoMapSample = AlbedoMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		surface.Albedo = albedoMapSample.rgb;
		alpha = albedoMapSample.a;
		if (IsMasked(MaterialConstantsGPU.Flags))
		{
			clip(alpha < 0.1f ? -1.0f : 1.0f);
		}
	}
	
	if (HaveNormalMap(MaterialConstantsGPU.Flags))
	{
		float4 normalMapSample = NormalMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		// Uncompress each component from [0,1] to [-1,1].
		float3 normalT = normalize(2.0f * normalMapSample.rgb - 1.0f);
		
		// Transform from tangent space to world space.
		inputPixel.normalW = normalize(mul(normalT, inputPixel.tbnMatrix));
	}
	
	if (HaveRoughnessMap(MaterialConstantsGPU.Flags))
	{
		float4 roughnessMapSample = RoughnessMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		surface.Roughness = roughnessMapSample.r;
	}
	
	if (HaveMetallicMap(MaterialConstantsGPU.Flags))
	{
		float4 metallicMapSample = MetallicMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		surface.Metallic = metallicMapSample.r;
	}
	
	if (HaveEmissiveMap(MaterialConstantsGPU.Flags))
	{
		float4 emissiveMapSample = EmissiveMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
		surface.Emissive = emissiveMapSample.rgb;
	}

    // Vector from point being lit to eye. 
	float3 viewDirection = normalize(RenderPassConstantsGPU.EyePosition - inputPixel.positionW);
	
	float3 color = surface.Emissive;
	
	int i;
	for (i = 0; i < RenderPassConstantsGPU.NumPointLights; ++i)
	{
		const PointLight pointLight = RenderPassConstantsGPU.PLights[i];
		
		float3 pixelToLight = pointLight.Position - inputPixel.positionW;
		float distance = length(pixelToLight);
		
		if (distance > pointLight.Radius)
			continue;

		pixelToLight /= distance;
		
		float pointAttenuation = saturate(1.0f - distance * distance / (pointLight.Radius * pointLight.Radius));
		pointAttenuation *= pointAttenuation;
		
		float3 radiance = pointLight.Strength * pointLight.Intensity * pointAttenuation;
	
		color += PBR(surface, inputPixel.normalW, viewDirection, pixelToLight, radiance);
	}

	for (i = 0; i < RenderPassConstantsGPU.NumSpotLights; ++i)
	{
		const SpotLight spotLight = RenderPassConstantsGPU.SLights[i];
		
		float3 pixelToLight = spotLight.Position - inputPixel.positionW;
		float distance = length(pixelToLight);
		pixelToLight /= distance;
		
		float pointAttenuation = saturate(1.0f - distance * distance / (spotLight.Radius * spotLight.Radius));
		pointAttenuation *= pointAttenuation;
		
		float spotRatio = saturate(dot(pixelToLight, spotLight.Direction));
		float spotAttenuation = saturate((spotRatio - spotLight.OuterConeAngle) / (spotLight.InnerConeAngle - spotLight.OuterConeAngle));
		spotAttenuation *= spotAttenuation;
		
		float3 radiance = spotLight.Strength * spotLight.Intensity * pointAttenuation * spotAttenuation;
	
		color += PBR(surface, inputPixel.normalW, viewDirection, pixelToLight, radiance);
	}
	
	int cascadeIndex = 0;
	for (i = NUM_CASCADES - 1; i >= 0; --i)
	{
		if (inputPixel.depth <= RenderPassConstantsGPU.Cascades[i].Split)
		{
			cascadeIndex = i;
		}
	}
	float shadowFactor = CalcShadowRatio(inputPixel.positionW, cascadeIndex, RenderPassConstantsGPU.Cascades, s_SamplerShadow, ShadowMap);
	float3 radiance = RenderPassConstantsGPU.DLight.Strength * RenderPassConstantsGPU.DLight.Intensity * shadowFactor;
	color += PBR(surface, inputPixel.normalW, viewDirection, -RenderPassConstantsGPU.DLight.Direction, radiance);
	
	// Ambient lighting (IBL)
	{
		float cosLo = max(0.0f, dot(inputPixel.normalW, viewDirection));
		
		// Specular reflection vector
		float3 irradiance = IrradianceMap.Sample(s_SamplerAnisotropicWrap, inputPixel.normalW).rgb;
		
		// Sample pre-filtered specular reflection environment at correct mipmap level
		float3 Lr = reflect(-viewDirection, inputPixel.normalW);
		uint width, height, mipLevel;
		PrefilteredRadianceMap.GetDimensions(0, width, height, mipLevel);
		float3 prefiltered = PrefilteredRadianceMap.SampleLevel(s_SamplerAnisotropicWrap, Lr, surface.Roughness * mipLevel).rgb;
		
		// Split-sum approximation factors for Cook-Torrance specular BRDF
		float2 brdfIntegration = BRDFLUT.Sample(s_SamplerLinearClamp, float2(cosLo, surface.Roughness)).rg;
		
		color += IBL(surface, cosLo, irradiance, prefiltered, brdfIntegration);
	}
	
	// Debug stuff
	if (RenderPassConstantsGPU.VisualizeCascade)
	{
		static const float3 vCascadeColorsMultiplier[NUM_CASCADES] =
		{
			float3(1.0f, 0.0f, 0.0f), // R
			float3(0.0f, 1.0f, 0.0f), // G
			float3(0.0f, 0.0f, 1.0f), // B
			float3(1.0f, 0.0f, 1.0f) // Magenta
		};
		
		color *= vCascadeColorsMultiplier[cascadeIndex];
	}
	
	if (RenderPassConstantsGPU.DebugViewInput > 0)
	{
		switch (RenderPassConstantsGPU.DebugViewInput)
		{
			case 1:
				color = surface.Albedo;
				break;
			case 2:
				color = inputPixel.normalW;
				break;
			case 3:
				color = surface.Roughness;
				break;
			case 4:
				color = surface.Metallic;
				break;
			case 5:
				color = surface.Emissive;
				break;
		}
	}
	
	return float4(color, alpha);
}