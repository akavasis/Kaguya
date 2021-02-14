#pragma once
#include <bitset>
#include "ThreadSafeQueue.h"

class Keyboard
{
public:
	friend class InputHandler;

	enum
	{
		BufferSize		= 16,
		NumKeyStates	= 256
	};

	struct Event
	{
		enum class EType
		{
			Invalid,
			Press,
			Release
		};

		struct SData
		{
			unsigned char Code;
		};

		Event() = default;
		Event(EType Type, unsigned char Code)
			: Type(Type)
		{
			Data.Code = Code;
		}

		EType Type = EType::Invalid;
		SData Data = {};
	};

	bool IsKeyPressed(unsigned char KeyCode) const;

	std::optional<Keyboard::Event> ReadKey();
	bool KeyBufferIsEmpty() const;

	unsigned char ReadChar();
	bool CharBufferIsEmpty() const;
private:
	void ResetKeyState();
	void OnKeyDown(unsigned char KeyCode);
	void OnKeyUp(unsigned char KeyCode);
	void OnChar(unsigned char Char);

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
	bool								AutoRepeat = false;
private:
	std::bitset<NumKeyStates>			m_KeyStates;
	ThreadSafeQueue<Keyboard::Event>	m_KeyBuffer;
	ThreadSafeQueue<unsigned char>		m_CharBuffer;

	friend class InputHandler;
};