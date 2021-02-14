#pragma once
#include <Core/RenderSystem.h>

#include "RaytracingAccelerationStructure.h"
#include "PathIntegrator.h"
#include "Picking.h"
#include "ToneMapper.h"

class Renderer : public RenderSystem
{
public:
	Renderer();
	Renderer(const Renderer&) = delete;
	Renderer& operator=(const Renderer&) = delete;
	~Renderer() override;

	Descriptor GetViewportDescriptor();
	Entity GetSelectedEntity();

	void SetViewportMousePosition(float MouseX, float MouseY);
	void SetViewportResolution(uint32_t Width, uint32_t Height);
protected:
	void Initialize() override;
	void Update(const Time& Time) override;
	void Render(Scene& Scene) override;
	void Resize(uint32_t Width, uint32_t Height) override;
	void Destroy() override;
	void RequestCapture() override;
private:
	float ViewportMouseX, ViewportMouseY;
	uint32_t ViewportWidth, ViewportHeight;
	D3D12_VIEWPORT Viewport;
	D3D12_RECT ScissorRect;

	RaytracingAccelerationStructure			RaytracingAccelerationStructure;
	PathIntegrator							PathIntegrator;
	Picking									Picking;
	ToneMapper								ToneMapper;

	std::shared_ptr<Resource> Materials;
	HLSL::Material* pMaterials = nullptr;
};