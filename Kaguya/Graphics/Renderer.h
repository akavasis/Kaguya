#pragma once
#include "Core/RenderSystem.h"

#include "RenderDevice.h"
#include "ResourceManager.h"

#include "Scene.h"
#include "RTScene.h"

#include "PathIntegrator.h"
#include "ToneMapper.h"

//----------------------------------------------------------------------------------------------------
class Renderer : public RenderSystem
{
public:
	Renderer();
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

	Scene									Scene;
	RTScene									RTScene;
	INT										InstanceID							= -1;

	//PathIntegrator							PathIntegrator;
	//ToneMapper								ToneMapper;
};