#pragma once
#include <cstdint>

//----------------------------------------------------------------------------------------------------
class Time;

//----------------------------------------------------------------------------------------------------
class RenderSystem
{
public:
	struct Statistics
	{
		inline static uint64_t	TotalFrameCount		= 0;
		inline static uint64_t	FrameCount			= 0;
		inline static double	TimeElapsed			= 0.0;
		inline static double	FPS					= 0.0;
		inline static double	FPMS				= 0.0;
	};

	struct Settings
	{
		inline static bool		VSync				= true;
	};

	RenderSystem(uint32_t Width, uint32_t Height);
	virtual ~RenderSystem();

	bool OnInitialize();
	void OnUpdate(const Time& Time);
	void OnHandleInput(float DeltaTime);
	void OnHandleRawInput(float DeltaTime);
	void OnRender();
	bool OnResize(uint32_t Width, uint32_t Height);
	void OnDestroy();
protected:
	virtual bool Initialize() = 0;
	virtual void Update(const Time& Time) = 0;
	virtual void HandleInput(float DeltaTime) = 0;
	virtual void HandleRawInput(float DeltaTime) = 0;
	virtual void Render() = 0;
	virtual bool Resize(uint32_t Width, uint32_t Height) = 0;
	virtual void Destroy() = 0;
protected:
	uint32_t	Width;
	uint32_t	Height;
	float		AspectRatio;
	bool		Screenshot;
};