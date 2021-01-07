#include "pch.h"
#include "Keyboard.h"

bool Keyboard::IsKeyPressed(unsigned char KeyCode) const
{
	return m_KeyStates[KeyCode];
}

Keyboard::Event Keyboard::ReadKey()
{
	if (Keyboard::Event e;
		m_KeyBuffer.Dequeue(e, 0))
		return e;
	return Keyboard::Event(Keyboard::Event::EType::Invalid, NULL);
}

bool Keyboard::KeyBufferIsEmpty() const
{
	return m_KeyBuffer.IsEmpty();
}

unsigned char Keyboard::ReadChar()
{
	if (unsigned char e;
		m_CharBuffer.Dequeue(e, 0))
		return e;
	return NULL;
}

bool Keyboard::CharBufferIsEmpty() const
{
	return m_CharBuffer.IsEmpty();
}

void Keyboard::ResetKeyState()
{
	m_KeyStates.reset();
}

void Keyboard::OnKeyDown(unsigned char KeyCode)
{
	m_KeyStates[KeyCode] = true;
	m_KeyBuffer.Enqueue(Keyboard::Event(Keyboard::Event::EType::Press, KeyCode));
	TrimBuffer(m_KeyBuffer);
}

void Keyboard::OnKeyUp(unsigned char KeyCode)
{
	m_KeyStates[KeyCode] = false;
	m_KeyBuffer.Enqueue(Keyboard::Event(Keyboard::Event::EType::Release, KeyCode));
	TrimBuffer(m_KeyBuffer);
}

void Keyboard::OnChar(unsigned char Char)
{
	m_CharBuffer.Enqueue(Char);
	TrimBuffer(m_CharBuffer);
}