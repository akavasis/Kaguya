#pragma once
#include "RenderDevice.h"

class RenderEngine
{
public:
	RenderEngine();
	~RenderEngine();

private:
	RenderDevice m_RenderDevice;
};