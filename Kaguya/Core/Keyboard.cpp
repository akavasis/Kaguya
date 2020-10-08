#include "pch.h"
#include "Keyboard.h"

Keyboard::Keyboard()
{
	m_AutoRepeat = false;
}

void Keyboard::EnableAutoRepeat()
{
	m_AutoRepeat = true;
}

void Keyboard::DisableAutoRepeat()
{
	m_AutoRepeat = false;
}

bool Keyboard::AutoRepeatEnabled() const
{
	return m_AutoRepeat;
}

bool Keyboard::IsKeyPressed(unsigned char KeyCode) const
{
	return m_KeyStates[KeyCode].load();
}

Keyboard::Event Keyboard::ReadKey()
{
	if (Keyboard::Event e;
		m_KeyBuffer.pop(e, 0))
		return e;
	return Keyboard::Event(Keyboard::Event::Type::Invalid, NULL);
}

bool Keyboard::KeyBufferIsEmpty() const
{
	return m_KeyBuffer.empty();
}

unsigned char Keyboard::ReadChar()
{
	if (unsigned char e;
		m_CharBuffer.pop(e, 0))
		return e;
	return NULL;
}

bool Keyboard::CharBufferIsEmpty() const
{
	return m_CharBuffer.empty();
}

void Keyboard::ResetKeyState()
{
	for (auto& keyState : m_KeyStates)
	{
		keyState = false;
	}
}

void Keyboard::OnKeyPress(unsigned char KeyCode)
{
	m_KeyStates[KeyCode] = true;
	m_KeyBuffer.push(Keyboard::Event(Keyboard::Event::Type::Press, KeyCode));
	TrimBuffer(m_KeyBuffer, s_BufferSize);
}

void Keyboard::OnKeyRelease(unsigned char KeyCode)
{
	m_KeyStates[KeyCode] = false;
	m_KeyBuffer.push(Keyboard::Event(Keyboard::Event::Type::Release, KeyCode));
	TrimBuffer(m_KeyBuffer, s_BufferSize);
}

void Keyboard::OnChar(unsigned char Char)
{
	m_CharBuffer.push(Char);
	TrimBuffer(m_CharBuffer, s_BufferSize);
}