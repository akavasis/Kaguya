#include "pch.h"
#include "RenderSystem.h"

#include "Time.h"

RenderSystem::RenderSystem(uint32_t Width, uint32_t Height)
	: Width(Width), Height(Height),
	AspectRatio(static_cast<float>(Width) / static_cast<float>(Height)),
	Screenshot(false)
{

}

RenderSystem::~RenderSystem()
{

}

bool RenderSystem::OnInitialize()
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

void RenderSystem::OnHandleMouse(bool CursorEnabled, int32_t X, int32_t Y, const Mouse& Mouse, float DeltaTime)
{
	// X and Y are raw deltas, hence why we have a separate parameter for it rather than
	// using the Mouse's X and Y
	return HandleMouse(CursorEnabled, X, Y, Mouse, DeltaTime);
}

void RenderSystem::OnHandleKeyboard(const Keyboard& Keyboard, float DeltaTime)
{
	return HandleKeyboard(Keyboard, DeltaTime);
}

void RenderSystem::OnRender()
{
	return Render();
}

bool RenderSystem::OnResize(uint32_t Width, uint32_t Height)
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