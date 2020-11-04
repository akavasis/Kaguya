#pragma once
#include "AL/D3D12/Fence.h"
#include "AL/D3D12/CommandQueue.h"

class RenderEngine
{
public:
	enum Type
	{
		Graphics,
		Compute,
		Copy
	};

private:
	Fence m_Fence;
	CommandQueue m_CommandQueue;
};