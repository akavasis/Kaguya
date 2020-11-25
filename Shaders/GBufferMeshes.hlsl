#include "HLSLCommon.hlsli"
#include "GBuffer.hlsli"

//----------------------------------------------------------------------------------------------------
cbuffer RootConstants
{
	uint InstanceID;
};

StructuredBuffer<Vertex> VertexBuffer	: register(t0, space0);
StructuredBuffer<uint> IndexBuffer		: register(t1, space0);
StructuredBuffer<Mesh> Meshes			: register(t2, space0);
StructuredBuffer<Material> Materials	: register(t3, space0);

#include <ShaderLayout.hlsli>

SamplerState AnisotropicClamp			: register(s0, space0);

struct VSOutput
{
	float4 PositionH			: SV_POSITION;
	float4 PreviousPositionH	: PREVIOUS_POSITIONH;
	float3 PositionW			: POSITIONW;
	float3 NormalL				: NORMALL;
	float2 TextureCoord			: TEXCOORD;
	float3x3 TBNMatrix			: TBNBASIS;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		Mesh mesh = Meshes[InstanceID];
		uint index = IndexBuffer[mesh.IndexOffset + VertexID];
		Vertex vertex = VertexBuffer[mesh.VertexOffset + index];
		
		float4 worldPos		= mul(float4(vertex.Position, 1.0f), mesh.World);
		float4 prevWorldPos = mul(float4(vertex.Position, 1.0f), mesh.PreviousWorld);
		
		// Transform to world space
		OUT.PositionW = worldPos.xyz;
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), g_SystemConstants.Camera.ViewProjection);
		
		OUT.PreviousPositionH = mul(prevWorldPos, g_SystemConstants.PreviousCamera.ViewProjection);
		
		OUT.NormalL = vertex.Normal;
		
		OUT.TextureCoord = vertex.Texture;
		// Transform normal to world space
		// Build orthonormal basis.
		float3 N = normalize(mul(vertex.Normal, (float3x3) mesh.World));
	
		OUT.TBNMatrix = GetTBNMatrix(N);
	}
	return OUT;
}

float3 GetAlbedoMap(VSOutput IN, Material Material)
{
	if (Material.UseAttributeAsValues)
		return Material.Albedo;
	
	int Index = Material.TextureIndices[AlbedoIdx];
	Texture2D AlbedoMap = g_Texture2DTable[Index];
	return AlbedoMap.Sample(AnisotropicClamp, IN.TextureCoord).rgb;
	return Material.Albedo;
}

float3 GetNormalMap(VSOutput IN, Material Material)
{
	int Index = Material.TextureIndices[NormalIdx];
	Texture2D NormalMap = g_Texture2DTable[Index];
	float3 Normal = NormalMap.Sample(AnisotropicClamp, IN.TextureCoord).xyz;
	Normal = Normal * 2.0f - 1.0f;
	return normalize(mul(Normal, IN.TBNMatrix));
}

float GetRoughnessMap(VSOutput IN, Material Material)
{
	if (Material.UseAttributeAsValues)
		return Material.Roughness;
	
	int Index = Material.TextureIndices[RoughnessIdx];
	Texture2D RoughnessMap = g_Texture2DTable[Index];
	
	switch (Material.TextureChannel[RoughnessIdx])
	{
		case TEXTURE_CHANNEL_RED:	return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
		case TEXTURE_CHANNEL_GREEN: return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).g;
		case TEXTURE_CHANNEL_BLUE:	return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).b;
		case TEXTURE_CHANNEL_ALPHA: return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).a;
		default:					return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
	}
}

float GetMetallicMap(VSOutput IN, Material Material)
{
	if (Material.UseAttributeAsValues)
		return Material.Metallic;
	
	int Index = Material.TextureIndices[MetallicIdx];
	Texture2D MetallicMap = g_Texture2DTable[Index];
	
	switch (Material.TextureChannel[MetallicIdx])
	{
		case TEXTURE_CHANNEL_RED:	return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
		case TEXTURE_CHANNEL_GREEN: return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).g;
		case TEXTURE_CHANNEL_BLUE:	return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).b;
		case TEXTURE_CHANNEL_ALPHA: return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).a;
		default:					return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
	}
}

// A simple utility to convert a float to a 2-component octohedral representation packed into one uint
uint dirToOct(float3 normal)
{
	float2 p = normal.xy * (1.0 / dot(abs(normal), 1.0.xxx));
	float2 e = normal.z > 0.0 ? p : (1.0 - abs(p.yx)) * (step(0.0, p) * 2.0 - (float2) (1.0));
	return (asuint(f32tof16(e.y)) << 16) + (asuint(f32tof16(e.x)));
}

// Take current clip position, last frame pixel position and compute a motion vector
float2 calcMotionVector(float4 prevClipPos, float2 currentPixelPos, float2 invFrameSize)
{
	float2 prevPosNDC = (prevClipPos.xy / prevClipPos.w) * float2(0.5, -0.5) + float2(0.5, 0.5);
	float2 motionVec = prevPosNDC - (currentPixelPos * invFrameSize);

	// Guard against inf/nan due to projection by w <= 0.
	const float epsilon = 1e-5f;
	motionVec = (prevClipPos.w < epsilon) ? float2(0, 0) : motionVec;
	return motionVec;
}

MRT PSMain(VSOutput IN)
{
	Mesh mesh = Meshes[InstanceID];
	Material material = Materials[mesh.MaterialIndex];
	
	GBufferMesh gbufferMesh = (GBufferMesh) 0;
	gbufferMesh.Albedo = GetAlbedoMap(IN, material);
	gbufferMesh.Normal = GetNormalMap(IN, material);
	gbufferMesh.Roughness = GetRoughnessMap(IN, material);
	gbufferMesh.Metallic = GetMetallicMap(IN, material);
	gbufferMesh.MaterialIndex = mesh.MaterialIndex;
	
	// The 'linearZ' buffer
	float linearZ = IN.PositionH.z * IN.PositionH.w;
	float maxChangeZ = max(abs(ddx(linearZ)), abs(ddy(linearZ)));
	float objNorm = asfloat(dirToOct(normalize(IN.NormalL)));
	float4 svgfLinearZOut = float4(linearZ, maxChangeZ, IN.PreviousPositionH.z, objNorm);

	// The 'motion vector' buffer
	float2 svgfMotionVec = calcMotionVector(IN.PreviousPositionH, IN.PositionH.xy, g_SystemConstants.OutputSize.zw);
	float2 posNormFWidth = float2(length(fwidth(IN.PositionW)), length(fwidth(gbufferMesh.Normal)));
	float4 svgfMotionVecOut = float4(svgfMotionVec, posNormFWidth);

	MRT OUT;
	OUT.Albedo				= float4(gbufferMesh.Albedo, gbufferMesh.Metallic);
	OUT.Normal				= float4(EncodeNormal(gbufferMesh.Normal), gbufferMesh.Roughness);
	OUT.TypeAndIndex		= (GBufferTypeMesh << 4) | (mesh.MaterialIndex & 0x0000000F);
	OUT.SVGF_LinearZ		= svgfLinearZOut;
	OUT.SVGF_MotionVector	= svgfMotionVecOut;
	OUT.SVGF_Compact		= float4(asfloat(dirToOct(gbufferMesh.Normal)), linearZ, maxChangeZ, 0.0f);
	return OUT;
}