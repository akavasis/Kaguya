#pragma once
#include "ThreadSafeQueue.h"

class Mouse
{
public:
	friend class Window;

	enum
	{
		BufferSize = 16
	};

	struct Event
	{
		enum class Type
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
		} type;
		struct Data
		{
			int X; int Y;
			bool LeftMouseButtonIsPressed;
			bool MiddleMouseButtonIsPressed;
			bool RightMouseButtonIsPressed;
		} data;

		Event() = default;
		Event(Type type, const Mouse& mouse)
			: type(type)
		{
			data.X = mouse.m_X;
			data.Y = mouse.m_Y;
			data.LeftMouseButtonIsPressed = mouse.m_LeftMouseButtonIsPressed;
			data.MiddleMouseButtonIsPressed = mouse.m_MiddleMouseButtonIsPressed;
			data.RightMouseButtonIsPressed = mouse.m_RightMouseButtonIsPressed;
		}
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

	bool LMBIsPressed() const;
	bool MMBIsPressed() const;
	bool RMBIsPressed() const;
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
		while (QueueBuffer.size() > BufferSize)
		{
			T item;
			QueueBuffer.pop(item, 0);
		}
	}

	int								m_X;
	int								m_Y;
	bool							m_LeftMouseButtonIsPressed;
	bool							m_MiddleMouseButtonIsPressed;
	bool							m_RightMouseButtonIsPressed;
	bool							m_IsInWindow;
	int								m_WheelDeltaCarry;
	bool							m_RawInputEnabled;
	ThreadSafeQueue<Mouse::Event>	m_MouseBuffer;
	ThreadSafeQueue<RawDelta>		m_RawDeltaBuffer;
};