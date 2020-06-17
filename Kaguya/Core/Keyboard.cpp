#include "pch.h"
#include "Keyboard.h"

Keyboard::Event::Event()
	: type(Keyboard::Event::Type::Invalid)
{
	data.Code = NULL;
}

Keyboard::Event::Event(Type type, unsigned char code)
	: type(type)
{
	data.Code = code;
}

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
	return m_KeyStates[KeyCode];
}

Keyboard::Event Keyboard::ReadKey()
{
	if (m_KeyBuffer.empty())
		return Keyboard::Event();
	Keyboard::Event e = m_KeyBuffer.front();
	m_KeyBuffer.pop();
	return e;
}

bool Keyboard::KeyBufferIsEmpty() const
{
	return m_KeyBuffer.empty();
}

unsigned char Keyboard::ReadChar()
{
	if (m_CharBuffer.empty())
		return NULL;
	unsigned char e = m_CharBuffer.front();
	m_CharBuffer.pop();
	return e;
}

bool Keyboard::CharBufferIsEmpty() const
{
	return m_CharBuffer.empty();
}

void Keyboard::ResetKeyState()
{
	m_KeyStates.reset();
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