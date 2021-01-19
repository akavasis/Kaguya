#pragma once
#include "RenderDevice.h"
#include "ResourceManager.h"
#include "Scene/Scene.h"

struct RTGeometry
{
	Microsoft::WRL::ComPtr<ID3D12Resource> Scratch;
	Microsoft::WRL::ComPtr<ID3D12Resource> Result;
	BottomLevelAccelerationStructure BottomLevelAccelerationStructure;
};

class RTScene
{
public:
	void Create(RenderDevice* pRenderDevice,
		ResourceManager* pResourceManager,
		Scene* pScene);

	void AddRTGeometry(const Mesh& Mesh);

	void Build(CommandList& CommandList);
private:
	RenderDevice* pRenderDevice;
	ResourceManager* pResourceManager;
	Scene* pScene;

	RootSignature RS;
	PipelineState PSO;

	std::vector<RTGeometry> Geometries;
	TopLevelAccelerationStructure TopLevelAccelerationStructure;

	Microsoft::WRL::ComPtr<ID3D12Resource> TLASScratch, TLASResult;
};