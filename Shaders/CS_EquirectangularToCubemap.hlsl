// Credit: https://github.com/jpvanoosten/LearningDirectX12/blob/master/DX12Lib/Resources/Shaders/PanoToCubemap_CS.hlsl
/**
 * This compute shader is used to convert a panoramic (equirectangular) image into a cubemap.
 */

#define BLOCK_SIZE 16

struct ComputeShaderInput
{
	uint3 GroupID : SV_GroupID; // 3D index of the thread group in the dispatch.
	uint3 GroupThreadID : SV_GroupThreadID; // 3D index of local thread ID in a thread group.
	uint3 DispatchThreadID : SV_DispatchThreadID; // 3D index of global thread ID in the dispatch.
	uint GroupIndex : SV_GroupIndex; // Flattened local index of the thread within a thread group.
};

cbuffer EquirectangularToCubemapCB : register(b0)
{
	uint CubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	uint FirstMip;					// The first mip level to generate.
	uint NumMips;					// The number of mips to generate.

	// Source texture as an equirectangular panoramic image.
    // It is assumed that the src texture has a full mipmap chain.
	int InputIndex;

	// Destination texture as a mip slice in the cubemap texture (texture array with 6 elements).
	int Output1Index;
	int Output2Index;
	int Output3Index;
	int Output4Index;
    int Output5Index;
}

SamplerState LinearRepeatSampler : register(s0);

// Shader layout define and include
#include "ShaderLayout.hlsli"

// 1 / PI
static const float InvPI = 0.31830988618379067153776752674503f;
static const float Inv2PI = 0.15915494309189533576888376337251f;
static const float2 InvAtan = float2(Inv2PI, InvPI);

// Transform from dispatch ID to cubemap face direction
static const float3x3 RotateUV[6] =
{
    // +X
	float3x3(0, 0, 1,
               0, -1, 0,
              -1, 0, 0),
    // -X
    float3x3(0, 0, -1,
               0, -1, 0,
               1, 0, 0),
    // +Y
    float3x3(1, 0, 0,
               0, 0, 1,
               0, 1, 0),
    // -Y
    float3x3(1, 0, 0,
               0, 0, -1,
               0, -1, 0),
    // +Z
    float3x3(1, 0, 0,
               0, -1, 0,
               0, 0, 1),
    // -Z
    float3x3(-1, 0, 0,
               0, -1, 0,
               0, 0, -1)
};

[numthreads(BLOCK_SIZE, BLOCK_SIZE, 1)]
void main(ComputeShaderInput IN)
{
    // Cubemap texture coords.
	uint3 texCoord = IN.DispatchThreadID;
    Texture2D SrcTexture = Texture2DTable[InputIndex];

    // First check if the thread is in the cubemap dimensions.
	if (texCoord.x >= CubemapSize || texCoord.y >= CubemapSize)
		return;

    // Map the UV coords of the cubemap face to a direction
    // [(0, 0), (1, 1)] => [(-0.5, -0.5), (0.5, 0.5)]
	float3 dir = float3(texCoord.xy / float(CubemapSize) - 0.5f, 0.5f);

    // Rotate to cubemap face
	dir = normalize(mul(RotateUV[texCoord.z], dir));

    // Convert the world space direction into U,V texture coordinates in the panoramic texture.
    // Source: http://gl.ict.usc.edu/Data/HighResProbes/
	float2 panoUV = float2(atan2(-dir.x, -dir.z), acos(dir.y)) * InvAtan;

    if (Output1Index != -1)
	{
        RWTexture2DArray<float4> DstMip1 = RWTexture2DArrayTable[Output1Index];
	    DstMip1[texCoord] = SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMip);
    }

    // Only perform on threads that are a multiple of 2.
	if (NumMips > 1 && (IN.GroupIndex & 0x11) == 0 &&
        Output2Index != -1)
	{
        RWTexture2DArray<float4> DstMip2 = RWTexture2DArrayTable[Output2Index];
		DstMip2[uint3(texCoord.xy / 2, texCoord.z)] = SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMip + 1);
	}

    // Only perform on threads that are a multiple of 4.
	if (NumMips > 2 && (IN.GroupIndex & 0x33) == 0 &&
        Output3Index != -1)
	{
        RWTexture2DArray<float4> DstMip3 = RWTexture2DArrayTable[Output3Index];
		DstMip3[uint3(texCoord.xy / 4, texCoord.z)] = SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMip + 2);
	}

    // Only perform on threads that are a multiple of 8.
	if (NumMips > 3 && (IN.GroupIndex & 0x77) == 0 &&
        Output4Index != -1)
	{
        RWTexture2DArray<float4> DstMip4 = RWTexture2DArrayTable[Output4Index];
		DstMip4[uint3(texCoord.xy / 8, texCoord.z)] = SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMip + 3);
	}

    // Only perform on threads that are a multiple of 16.
    // This should only be thread 0 in this group.
	if (NumMips > 4 && (IN.GroupIndex & 0xFF) == 0 &&
        Output5Index != -1)
	{
        RWTexture2DArray<float4> DstMip5 = RWTexture2DArrayTable[Output5Index];
		DstMip5[uint3(texCoord.xy / 16, texCoord.z)] = SrcTexture.SampleLevel(LinearRepeatSampler, panoUV, FirstMip + 4);
	}
}