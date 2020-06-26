#include "HLSLCommon.hlsli"
#include "StaticSamplers.hlsli"

ConstantBuffer<Object> ObjectConstantsGPU : register(b0);
ConstantBuffer<Material> MaterialConstantsGPU : register(b1);
ConstantBuffer<RenderPass> RenderPassConstantsGPU : register(b2);

Texture2DArray ShadowMap : register(t0);

Texture2D AlbedoMap : register(t1);
Texture2D NormalMap : register(t2);
Texture2D RoughnessMap : register(t3);
Texture2D MetallicMap : register(t4);

struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 positionW : POSITION;
	float2 textureCoord : TEXCOORD;
	float3 normalW : NORMAL;
	float3 tangentW : TANGENT;
	float depth : DEPTH;
};
float4 main(OutputVertex inputPixel) : SV_TARGET
{
	//inputPixel.normalW = normalize(inputPixel.normalW);
	//inputPixel.tangentW = normalize(inputPixel.tangentW);
	
	//MaterialSurfaceBlinnPhong material;
	//material.Diffuse = MaterialConstantsGPU.Albedo;
	//material.SpecularIntensity = MaterialConstantsGPU.SpecularIntensity;
	//material.Shininess = MaterialConstantsGPU.Shininess;
	//material.Specular = MaterialConstantsGPU.Specular;
	
	//float alpha = 1.0f;
	//float4 diffuseMapSample = float4(1.0f, 1.0f, 1.0f, 1.0f);
	//if (HaveDiffuseTexture(MaterialConstantsGPU.Flags))
	//{
	//	diffuseMapSample = DiffuseMap.Sample(s_SamplerAnisotropicWrap, inputPixel.textureCoord);
	//	alpha = diffuseMapSample.a;
	//	if (IsMasked(MaterialConstantsGPU.Flags))
	//	{
	//		clip(alpha < 0.1f ? -1.0f : 1.0f);
	//	}
	//}
	
	//if (HaveSpecularTexture(MaterialConstantsGPU.Flags))
	//{
	//	float4 specularMapSample = SpecularMap.Sample(s_SamplerLinearWrap, inputPixel.textureCoord);
	//	material.Specular = specularMapSample.rgb;
	//}
	
	//if (HaveNormalTexture(MaterialConstantsGPU.Flags))
	//{
	//	float4 normalMapSample = NormalMap.Sample(s_SamplerLinearWrap, inputPixel.textureCoord);
		
	//	// Uncompress each component from [0,1] to [-1,1].
	//	float3 normalT = 2.0f * normalMapSample.rgb - 1.0f;
		
	//	// Build orthonormal basis.
	//	float3 N = inputPixel.normalW;
	//	// re-orthogonalize T with respect to N, using Gram-Schmidt process
	//	float3 T = normalize(inputPixel.tangentW - dot(inputPixel.tangentW, N) * N);
	//	// then retrieve perpendicular vector B with the cross product of T and N
	//	float3 B = cross(N, T);
	//	float3x3 TBN = float3x3(T, B, N);

	//	// Transform from tangent space to world space.
	//	inputPixel.normalW = mul(normalT, TBN);
	//}

 //   // Vector from point being lit to eye. 
	//float3 viewDirection = normalize(RenderPassConstantsGPU.EyePosition - inputPixel.positionW);
	
	//LightResult total = (LightResult) 0;
	//total.Diffuse = RenderPassConstantsGPU.AmbientLight;
	
	//int i;
	//for (i = 0; i < RenderPassConstantsGPU.NumPointLights; ++i)
	//{
	//	LightResult pointLightResult = CalcPointLight(RenderPassConstantsGPU.PLights[i], viewDirection, inputPixel.positionW, inputPixel.normalW, material);
		
	//	total.Diffuse += pointLightResult.Diffuse;
	//	total.Specular += pointLightResult.Specular;
	//}

	//for (i = 0; i < RenderPassConstantsGPU.NumSpotLights; ++i)
	//{
	//	LightResult spotLightResult = CalcSpotLight(RenderPassConstantsGPU.SLights[i], viewDirection, inputPixel.positionW, inputPixel.normalW, material);
		
	//	total.Diffuse += spotLightResult.Diffuse;
	//	total.Specular += spotLightResult.Specular;
	//}
	
	//int cascadeIndex = 0;
	//for (i = NUM_CASCADES - 1; i >= 0; --i)
	//{
	//	if (inputPixel.depth <= RenderPassConstantsGPU.Cascades[i].Split)
	//	{
	//		cascadeIndex = i;
	//	}
	//}
	//float shadowFactor = CalcShadowRatio(inputPixel.positionW, cascadeIndex, RenderPassConstantsGPU.Cascades, s_SamplerShadow, ShadowMap);
	//LightResult directionalLightResult = CalcDirectionalLight(RenderPassConstantsGPU.DLight, viewDirection, inputPixel.positionW, inputPixel.normalW, material);
	//directionalLightResult.Diffuse *= shadowFactor;
	//directionalLightResult.Specular *= shadowFactor;
	
	//total.Diffuse += directionalLightResult.Diffuse;
	//total.Specular += directionalLightResult.Specular;
	
	//if (RenderPassConstantsGPU.VisualizeCascade)
	//{
	//	static const float3 vCascadeColorsMultiplier[NUM_CASCADES] =
	//	{
	//		float3(1.0f, 0.0f, 0.0f), // R
	//		float3(0.0f, 1.0f, 0.0f), // G
	//		float3(0.0f, 0.0f, 1.0f), // B
	//		float3(1.0f, 0.0f, 1.0f) // Magenta
	//	};
		
	//	total.Diffuse *= vCascadeColorsMultiplier[cascadeIndex];
	//	total.Specular *= vCascadeColorsMultiplier[cascadeIndex];
	//}
	
	//const float3 diffuse = total.Diffuse;
	//const float3 specular = total.Specular;
	
	//return float4(diffuse * diffuseMapSample.xyz + specular, alpha);
	return float4(inputPixel.normalW, 1.0f);
}