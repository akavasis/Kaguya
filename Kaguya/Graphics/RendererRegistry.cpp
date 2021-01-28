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
		VS::FullScreenTriangle			= ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/FullScreenTriangle.hlsl", VSEntryPoint, {});
	}

	// Load PS
	{
		PS::ToneMap						= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/ToneMap.hlsl",	PSEntryPoint, {});
	}

	// Load CS
	{
		//CS::InstanceGeneration			= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/InstanceGeneration.hlsl",		CSEntryPoint, {});

		//CS::PostProcess_BloomMask						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl",						CSEntryPoint, {});
		//CS::PostProcess_BloomDownsample					= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl",				CSEntryPoint, {});
		//CS::PostProcess_BloomBlur						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl",						CSEntryPoint, {});
		//CS::PostProcess_BloomUpsampleBlurAccumulation	= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl",	CSEntryPoint, {});
		//CS::PostProcess_BloomComposition				= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl",				CSEntryPoint, {});
	}
}

void Libraries::Register(const ShaderCompiler& ShaderCompiler)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	PathTrace = ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/PathTrace.hlsl");
	Picking = ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Picking.hlsl");
}