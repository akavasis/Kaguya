#include "pch.h"
#include "RenderSystem.h"

#include "Time.h"

RenderSystem::RenderSystem(uint32_t Width, uint32_t Height)
	: Width(Width)
	, Height(Height)
	, AspectRatio(static_cast<float>(Width) / static_cast<float>(Height))
{
	Settings::RestoreDefaults();
}

RenderSystem::~RenderSystem()
{

}

void RenderSystem::OnInitialize()
{
	return Initialize();
}

void RenderSystem::OnUpdate(const Time& Time)
{
	Statistics::TotalFrameCount++;
	Statistics::FrameCount++;
	if (Time.TotalTime() - Statistics::TimeElapsed >= 1.0)
	{
		Statistics::FPS = static_cast<double>(Statistics::FrameCount);
		Statistics::FPMS = 1000.0 / Statistics::FPS;
		Statistics::FrameCount = 0;
		Statistics::TimeElapsed += 1.0;
	}
	return Update(Time);
}

void RenderSystem::OnRender(Scene& Scene)
{
	return Render(Scene);
}

void RenderSystem::OnResize(uint32_t Width, uint32_t Height)
{
	this->Width = Width;
	this->Height = Height;
	AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	return Resize(Width, Height);
}

void RenderSystem::OnDestroy()
{
	return Destroy();
}

void RenderSystem::OnRequestCapture()
{
	return RequestCapture();
}