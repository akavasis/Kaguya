#pragma once
#include "Core/RenderSystem.h"

#include "RenderDevice.h"
#include "ResourceManager.h"

#include "RTScene.h"

#include "Editor.h"

#include "PathIntegrator.h"
#include "ToneMapper.h"

class Renderer : public RenderSystem
{
public:
	Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	~Renderer() override;
protected:
	bool Initialize() override;
	void Update(const Time& Time) override;
	void HandleInput(float DeltaTime) override;
	void HandleRawInput(float DeltaTime) override;
	void Render() override;
	bool Resize(uint32_t Width, uint32_t Height) override;
	void Destroy() override;
private:
	void RenderGui();
private:
	RenderDevice							RenderDevice;
	ResourceManager							ResourceManager;

	// Realistically, Editor and Renderer should be separate,
	// but because we are using ImGui and rendering directly into the back buffer
	// the Renderer contains Editor...
	// TODO: Renderer and Editor should be separate, come up with an alternative...
	// (e.g. render onto offscreen texture and composite them)
	Scene									Scene;
	RaytracingAccelerationStructure			RaytracingAccelerationStructure;
	Editor									Editor;
	PathIntegrator							PathIntegrator;
	ToneMapper								ToneMapper;

	std::shared_ptr<Resource> Materials;
	HLSL::Material* pMaterials = nullptr;
};