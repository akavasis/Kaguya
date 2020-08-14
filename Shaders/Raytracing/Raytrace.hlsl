#include "Global.hlsli"

struct HitInfo
{
	uint GeometryIndex;
};
ConstantBuffer<HitInfo> HitGroupCB : register(b0, space0);

[shader("raygeneration")]
void RayGen()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions);
	float2 ndcCoord = pixel * 2.0f - 1.0f; // Uncompress each component from [0,1] to [-1,1].
	ndcCoord.y *= -1.0f;
  
  	// Initialize the ray payload
	RayPayload payload;
	payload.colorAndDistance = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
  	// Transform from screen space to projection space
	RayDesc ray;
	ray.Origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), RenderPassDataCB.InvView).xyz;
	float4 target = mul(float4(ndcCoord, 1.0f, 1.0f), RenderPassDataCB.InvProjection);
	ray.Direction = mul(float4(target.xyz, 0.0f), RenderPassDataCB.InvView).xyz;
	ray.TMin = 0.001f;
	ray.TMax = 100000.0f;
    
  	// Trace the ray
	const uint flags = RAY_FLAG_NONE;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, payload);

	RenderTarget[launchIndex] = float4(payload.colorAndDistance.rgb, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
	const float3 rayDir = WorldRayDirection();

	TextureCube skybox = TexCubeTable[RenderPassDataCB.IrradianceCubemapIndex];
	float3 color = skybox.SampleLevel(SamplerLinearWrap, rayDir, 0.0f).xyz;

	payload.colorAndDistance = float4(color, -1.0f);
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload payload)
{
	payload.visibility = 1.0f;
}

Vertex GetHitSurface(in HitAttributes attrib, in uint geometryIndex)
{
	float3 barycentrics =
    float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);

	GeometryInfo info = GeometryInfoBuffer[geometryIndex];

	const uint primIndex = PrimitiveIndex();
	const uint idx0 = IndexBuffer[primIndex * 3 + info.IndexOffset + 0];
    const uint idx1 = IndexBuffer[primIndex * 3 + info.IndexOffset + 1];
    const uint idx2 = IndexBuffer[primIndex * 3 + info.IndexOffset + 2];
	
	const Vertex vtx0 = VertexBuffer[idx0 + info.VertexOffset];
    const Vertex vtx1 = VertexBuffer[idx1 + info.VertexOffset];
    const Vertex vtx2 = VertexBuffer[idx2 + info.VertexOffset];

	return BERP(vtx0, vtx1, vtx2, barycentrics);
}

MaterialTextureIndices GetMaterialIndices(in uint geometryIndex)
{
	GeometryInfo info = GeometryInfoBuffer[geometryIndex];
	return MaterialIndices[info.MaterialIndex];
}

MaterialTextureProperties GetMaterialProperties(in uint geometryIndex)
{
	GeometryInfo info = GeometryInfoBuffer[geometryIndex];
	return MaterialProperties[info.MaterialIndex];
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in HitAttributes attrib)
{
	Vertex hitSurface = GetHitSurface(attrib, HitGroupCB.GeometryIndex);
	MaterialTextureIndices materialIndices = GetMaterialIndices(HitGroupCB.GeometryIndex);
	MaterialTextureProperties materialProperties = GetMaterialProperties(HitGroupCB.GeometryIndex);

	const float3 incomingRayOriginWS = WorldRayOrigin();
    const float3 incomingRayDirWS = WorldRayDirection();

	// Initialize the ray payload
	ShadowRayPayload shadowRayPayload;
	shadowRayPayload.visibility = 1.0f;

	// Shoot a shadow ray
	RayDesc ray;
	ray.Origin = incomingRayOriginWS + incomingRayDirWS * RayTCurrent();
	ray.Direction = -RenderPassDataCB.Sun.Direction;
	ray.TMin = 0.001f;
	ray.TMax = 100000.0f;

	const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypeShadow;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypeShadow;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, shadowRayPayload);

	// Fetch albedo/diffuse data
	if (materialIndices.AlbedoMapIndex != -1)
	{
		Texture2D albedoMap = Tex2DTable[materialIndices.AlbedoMapIndex];
		float4 albedoMapSample = albedoMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Albedo = albedoMapSample.rgb;
	}
	
	// Fetch normal data
	if (materialIndices.NormalMapIndex != -1)
	{
		float3x3 tbnMatrix = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal); 

		Texture2D normalMap = Tex2DTable[materialIndices.NormalMapIndex];
		float4 normalMapSample = normalMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		float3 normalT = normalize(2.0f * normalMapSample.rgb - 1.0f); // Uncompress each component from [0,1] to [-1,1].
		hitSurface.Normal = normalize(mul(normalT, tbnMatrix)); // Transform from tangent space to world space.
	}
	
	// Fetch roughness data
	if (materialIndices.RoughnessMapIndex != -1)
	{
		Texture2D roughnessMap = Tex2DTable[materialIndices.RoughnessMapIndex];
		float4 roughnessMapSample = roughnessMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Roughness = roughnessMapSample.r;
	}
	
	// Fetch metallic data
	if (materialIndices.MetallicMapIndex != -1)
	{
		Texture2D metallicMap = Tex2DTable[materialIndices.MetallicMapIndex];
		float4 metallicMapSample = metallicMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Metallic = metallicMapSample.r;
	}
	
	// Fetch emissive data
	if (materialIndices.EmissiveMapIndex != -1)
	{
		Texture2D emissiveMap = Tex2DTable[materialIndices.EmissiveMapIndex];
		float4 emissiveMapSample = emissiveMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Emissive = emissiveMapSample.rgb;
	}

	float3 hitColor = materialProperties.Albedo;

	hitColor *= shadowRayPayload.visibility;
	payload.colorAndDistance = float4(hitColor, RayTCurrent());
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload payload, in HitAttributes attrib)
{
	payload.visibility = 0.0f;
}