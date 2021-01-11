#pragma once

#include <optional>
#include "ThreadSafeQueue.h"

class Mouse
{
public:
	friend class InputHandler;

	enum
	{
		BufferSize = 16
	};

	struct Event
	{
		enum class EType
		{
			Invalid,
			Move,
			Enter,
			Leave,
			LMBPress,
			LMBRelease,
			MMBPress,
			MMBRelease,
			RMBPress,
			RMBRelease,
			WheelUp,
			WheelDown
		};

		struct SData
		{
			int X; int Y;
			bool LeftMouseButtonIsPressed;
			bool MiddleMouseButtonIsPressed;
			bool RightMouseButtonIsPressed;
		};

		Event() = default;
		Event(EType Type, const Mouse& Mouse)
			: Type(Type)
		{
			Data.X = Mouse.X;
			Data.Y = Mouse.Y;
			Data.LeftMouseButtonIsPressed = Mouse.m_IsLMBPressed;
			Data.MiddleMouseButtonIsPressed = Mouse.m_IsMMBPressed;
			Data.RightMouseButtonIsPressed = Mouse.m_IsRMBPressed;
		}

		EType Type = EType::Invalid;
		SData Data = {};
	};

	struct RawInput
	{
		int X, Y;
	};

	Mouse();

	bool IsLMBPressed() const;
	bool IsMMBPressed() const;
	bool IsRMBPressed() const;
	bool IsInWindow() const;

	std::optional<Mouse::Event> Read();

	std::optional<Mouse::RawInput> ReadRawInput();
private:
	void OnMouseMove(int X, int Y);
	void OnMouseEnter();
	void OnMouseLeave();

	void OnLMBDown();
	void OnMMBDown();
	void OnRMBDown();

	void OnLMBUp();
	void OnMMBUp();
	void OnRMBUp();

	void OnWheelDown();
	void OnWheelUp();
	void OnWheelDelta(int WheelDelta);

	void OnRawInput(int X, int Y);

	template<class T>
	void TrimBuffer(ThreadSafeQueue<T>& QueueBuffer)
	{
		while (QueueBuffer.Size() > BufferSize)
		{
			T item;
			QueueBuffer.Dequeue(item, 0);
		}
	}

public:
	bool							UseRawInput = false;
	int								X			= 0;
	int								Y			= 0;
	int								RawX		= 0;
	int								RawY		= 0;

private:
	bool							m_IsLMBPressed;
	bool							m_IsMMBPressed;
	bool							m_IsRMBPressed;
	bool							m_IsInWindow;
	int								m_WheelDeltaCarry;
	ThreadSafeQueue<Mouse::Event>	m_MouseBuffer;
	ThreadSafeQueue<RawInput>		m_RawDeltaBuffer;
};