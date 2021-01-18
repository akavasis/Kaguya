#include "pch.h"
#include "RendererRegistry.h"

const LPCWSTR VSEntryPoint = L"VSMain";
const LPCWSTR MSEntryPoint = L"MSMain";
const LPCWSTR PSEntryPoint = L"PSMain";
const LPCWSTR CSEntryPoint = L"CSMain";

void Shaders::Register(const ShaderCompiler& ShaderCompiler)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	// Load VS
	{
		VS::Quad										= ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Quad.hlsl",		VSEntryPoint, {});
	}

	// Load PS
	{
		PS::PostProcess_Tonemapping						= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/Tonemapping.hlsl",	PSEntryPoint, {});
	}

	// Load CS
	{
		CS::InstanceGeneration							= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/InstanceGeneration.hlsl",		CSEntryPoint, {});

		CS::PostProcess_BloomMask						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomDownsample					= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl",				CSEntryPoint, {});
		CS::PostProcess_BloomBlur						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomUpsampleBlurAccumulation	= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl",	CSEntryPoint, {});
		CS::PostProcess_BloomComposition				= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl",				CSEntryPoint, {});
	}
}

void Libraries::Register(const ShaderCompiler& ShaderCompiler)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	Pathtracing			= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Pathtracing.hlsl");
	Picking				= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Picking.hlsl");
}

//void RootSignatures::Register(RenderDevice* pRenderDevice)
//{
//	pRenderDevice->CreateRootSignature(Default, [](RootSignatureBuilder& Builder)
//	{
//		Builder.DenyTessellationShaderAccess();
//		Builder.DenyGSAccess();
//	});
//
//	// Local Picking RS
//	pRenderDevice->CreateRootSignature(Raytracing::Local::Picking, [](RootSignatureBuilder& Builder)
//	{
//
//	},
//		false);
//
//	// Local RS
//	pRenderDevice->CreateRootSignature(Raytracing::Local::Default, [](RootSignatureBuilder& Builder)
//	{
//		Builder.AddRootSRVParameter(RootSRV(0, 1));	// VertexBuffer,			t0 | space1
//		Builder.AddRootSRVParameter(RootSRV(1, 1));	// IndexBuffer				t1 | space1
//
//		Builder.SetAsLocalRootSignature();
//	},
//		false);
//
//	// Global RS
//	pRenderDevice->CreateRootSignature(Raytracing::Global, [](RootSignatureBuilder& Builder)
//	{
//		Builder.AddRootSRVParameter(RootSRV(0, 0));	// BVH,						t0 | space0
//		Builder.AddRootSRVParameter(RootSRV(1, 0));	// Meshes,					t1 | space0
//		Builder.AddRootSRVParameter(RootSRV(2, 0));	// Lights,					t2 | space0
//		Builder.AddRootSRVParameter(RootSRV(3, 0));	// Materials				t3 | space0
//
//		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
//		Builder.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
//	});
//}