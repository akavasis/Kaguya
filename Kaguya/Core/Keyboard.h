#pragma once
#include <bitset>
#include "ThreadSafeQueue.h"

class Keyboard
{
public:
	friend class Window;

	enum
	{
		BufferSize		= 16,
		NumKeyStates	= 256
	};

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
	void ResetKeyState();
	void OnKeyPress(unsigned char KeyCode);
	void OnKeyRelease(unsigned char KeyCode);
	void OnChar(unsigned char Char);
	template<class T>
	void TrimBuffer(ThreadSafeQueue<T>& QueueBuffer)
	{
		while (QueueBuffer.size() > BufferSize)
		{
			T item;
			QueueBuffer.pop(item, 0);
		}
	}

	bool								m_AutoRepeat;
	std::bitset<NumKeyStates>			m_KeyStates;
	ThreadSafeQueue<Keyboard::Event>	m_KeyBuffer;
	ThreadSafeQueue<unsigned char>		m_CharBuffer;
};