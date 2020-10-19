#pragma once
#include <stdint.h>

//----------------------------------------------------------------------------------------------------
class Keyboard;
class Time;

//----------------------------------------------------------------------------------------------------
class RenderSystem
{
public:
	struct Statistics
	{
		inline static uint64_t	TotalFrameCount = 0;
		inline static uint64_t	FrameCount = 0;
		inline static double	TimeElapsed = 0.0;
		inline static double	FPS = 0.0;
		inline static double	FPMS = 0.0;
	};

	struct Settings
	{
		inline static bool		VSync = false;
	};

	RenderSystem(uint32_t Width, uint32_t Height);
	virtual ~RenderSystem();

	void OnInitialize();
	void OnHandleMouse(int32_t X, int32_t Y, float DeltaTime);
	void OnHandleKeyboard(const Keyboard& Keyboard, float DeltaTime);
	void OnUpdate(const Time& Time);
	void OnRender();
	void OnResize(uint32_t Width, uint32_t Height);
	void OnDestroy();
protected:
	virtual void Initialize() = 0;
	virtual void HandleMouse(int32_t X, int32_t Y, float DeltaTime) = 0;
	virtual void HandleKeyboard(const Keyboard& Keyboard, float DeltaTime) = 0;
	virtual void Update(const Time& Time) = 0;
	virtual void Render() = 0;
	virtual void Resize(uint32_t Width, uint32_t Height) = 0;
	virtual void Destroy() = 0;

	uint32_t Width;
	uint32_t Height;
	float AspectRatio;
};