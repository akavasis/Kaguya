#pragma once
#include "ThreadSafeQueue.h"

class Mouse
{
public:
	friend class Window;

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
	static constexpr unsigned int s_BufferSize = 16u;
	std::atomic<int> m_X;
	std::atomic<int> m_Y;
	std::atomic<bool> m_LeftMouseButtonIsPressed;
	std::atomic<bool> m_MiddleMouseButtonIsPressed;
	std::atomic<bool> m_RightMouseButtonIsPressed;
	std::atomic<bool> m_IsInWindow;
	std::atomic<int> m_WheelDeltaCarry;
	std::atomic<bool> m_RawInputEnabled;
	ThreadSafeQueue<Mouse::Event> m_MouseBuffer;
	ThreadSafeQueue<RawDelta> m_RawDeltaBuffer;

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
	void TrimBuffer(ThreadSafeQueue<T>& QueueBuffer, unsigned int Limit)
	{
		while (QueueBuffer.size() > Limit)
		{
			T item;
			QueueBuffer.pop(item, 0);
		}
	}
};