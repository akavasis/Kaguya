#pragma once
#include <bitset>
#include "ThreadSafeQueue.h"

class Keyboard
{
public:
	friend class Window;

	struct Event
	{
		enum class Type
		{
			Invalid,
			Press,
			Release
		} type;
		struct Data
		{
			unsigned char Code;
		} data;

		Event() = default;
		Event(Type type, unsigned char code)
			: type(type)
		{
			data.Code = code;
		}
	};

	Keyboard();
	~Keyboard() = default;

	void EnableAutoRepeat();
	void DisableAutoRepeat();
	bool AutoRepeatEnabled() const;

	bool IsKeyPressed(unsigned char KeyCode) const;

	Keyboard::Event ReadKey();
	bool KeyBufferIsEmpty() const;

	unsigned char ReadChar();
	bool CharBufferIsEmpty() const;
private:
	static constexpr unsigned int s_BufferSize = 16u;
	static constexpr unsigned int s_NumKeyStates = 256u;
	bool m_AutoRepeat;
	std::atomic<bool> m_KeyStates[s_NumKeyStates];
	ThreadSafeQueue<Keyboard::Event> m_KeyBuffer;
	ThreadSafeQueue<unsigned char> m_CharBuffer;

	void ResetKeyState();
	void OnKeyPress(unsigned char KeyCode);
	void OnKeyRelease(unsigned char KeyCode);
	void OnChar(unsigned char Char);
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