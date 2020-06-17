#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

enum HandleType
{
	Buffer = 0b00,
	Texture1D = 0b01,
	Texture2D = 0b10,
	Texture3D = 0b11
};

enum HandleFlag
{
	INACTIVE = 0b00000001,
	DESTROY = 0b00000010
};

struct RenderHandle
{
	UINT64 Type : 2;
	UINT64 Flag : 8;
	UINT64 Data : 54;
	bool operator==(const RenderHandle& rhs) const
	{
		return Data == rhs.Data;
	}
	bool operator!=(const RenderHandle& rhs) const
	{
		return Data != rhs.Data;
	}
};

void SetRenderHandleType(RenderHandle& RenderHandle, HandleType Type)
{
	RenderHandle.Type = Type;
}

void SetRenderHandleFlag(RenderHandle& RenderHandle, HandleFlag Flag)
{
	RenderHandle.Flag = Flag;
}

#define RENDER_HANDLE_MAX_DATA_SIZE (1 << 54)