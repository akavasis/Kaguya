#pragma once
#include <stdint.h>

//----------------------------------------------------------------------------------------------------
class Keyboard;
class Time;

//----------------------------------------------------------------------------------------------------
class RenderSystem
{
public:
	RenderSystem(uint32_t Width, uint32_t Height)
		: Width(Width), Height(Height)
	{
		UpdateForResize(Width, Height);
	}

	virtual ~RenderSystem() {}

	virtual void OnInitialize() = 0;
	virtual void OnHandleMouse(int32_t X, int32_t Y, float DeltaTime) = 0;
	virtual void OnHandleKeyboard(const Keyboard& Keyboard, float DeltaTime) = 0;
	virtual void OnUpdate(const Time& Time) = 0;
	virtual void OnRender() = 0;
	virtual void OnResize(uint32_t Width, uint32_t Height) = 0;
	virtual void OnDestroy() = 0;

	void UpdateForResize(uint32_t Width, uint32_t Height)
	{
		this->Width = Width;
		this->Height = Height;
		AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
	}
protected:
	uint32_t Width;
	uint32_t Height;
	float AspectRatio;
};