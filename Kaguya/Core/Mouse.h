#pragma once

#include "ThreadSafeQueue.h"

class Mouse
{
public:
	friend class Window;
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
			Data.X = Mouse.m_X;
			Data.Y = Mouse.m_Y;
			Data.LeftMouseButtonIsPressed = Mouse.m_IsLMBPressed;
			Data.MiddleMouseButtonIsPressed = Mouse.m_IsMMBPressed;
			Data.RightMouseButtonIsPressed = Mouse.m_IsRMBPressed;
		}

		EType Type = EType::Invalid;
		SData Data = {};
	};

	struct RawDelta
	{
		int X, Y;
	};

	Mouse();

	void EnableRawInput();
	void DisableRawInput();
	bool RawInputEnabled() const;

	int X() const;
	int Y() const;

	bool IsLMBPressed() const;
	bool IsMMBPressed() const;
	bool IsRMBPressed() const;
	bool IsInWindow() const;

	Mouse::Event Read();
	bool MouseBufferIsEmpty() const;

	Mouse::RawDelta ReadRawDelta();
	bool RawDeltaBufferIsEmpty() const;
private:
	void OnMouseMove(int x, int y);
	void OnMouseEnter();
	void OnMouseLeave();
	void OnLMBPress(int x, int y);
	void OnLMBRelease(int x, int y);
	void OnMMBPress(int x, int y);
	void OnMMBRelease(int x, int y);
	void OnRMBPress(int x, int y);
	void OnRMBRelease(int x, int y);
	void OnWheelUp(int x, int y);
	void OnWheelDown(int x, int y);
	void OnWheelDelta(int x, int y, int delta);
	void OnRawDelta(int dx, int dy);
	template<class T>
	void TrimBuffer(ThreadSafeQueue<T>& QueueBuffer)
	{
		while (QueueBuffer.Size() > BufferSize)
		{
			T item;
			QueueBuffer.Dequeue(item, 0);
		}
	}

private:
	int								m_X;
	int								m_Y;
	bool							m_IsLMBPressed;
	bool							m_IsMMBPressed;
	bool							m_IsRMBPressed;
	bool							m_IsInWindow;
	int								m_WheelDeltaCarry;
	bool							m_RawInputEnabled;
	ThreadSafeQueue<Mouse::Event>	m_MouseBuffer;
	ThreadSafeQueue<RawDelta>		m_RawDeltaBuffer;
};