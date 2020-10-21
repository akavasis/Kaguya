#include "pch.h"
#include "Mouse.h"

Mouse::Mouse()
{
	m_X = m_Y = 0;
	m_LeftMouseButtonIsPressed = m_MiddleMouseButtonIsPressed = m_RightMouseButtonIsPressed = m_IsInWindow = false;
	m_WheelDeltaCarry = 0;
	m_RawInputEnabled = false;
}

void Mouse::EnableRawInput()
{
	m_RawInputEnabled = true;
}

void Mouse::DisableRawInput()
{
	m_RawInputEnabled = false;
}

bool Mouse::RawInputEnabled() const
{
	return m_RawInputEnabled;
}

int Mouse::X() const
{
	return m_X;
}

int Mouse::Y() const
{
	return m_Y;
}

bool Mouse::LMBIsPressed() const
{
	return m_LeftMouseButtonIsPressed;
}

bool Mouse::MMBIsPressed() const
{
	return m_MiddleMouseButtonIsPressed;
}

bool Mouse::RMBIsPressed() const
{
	return m_RightMouseButtonIsPressed;
}

bool Mouse::IsInWindow() const
{
	return m_IsInWindow;
}

Mouse::Event Mouse::Read()
{
	if (Mouse::Event e;
		m_MouseBuffer.pop(e, 0))
		return e;
	return Mouse::Event(Mouse::Event::Type::Invalid, *this);
}

bool Mouse::MouseBufferIsEmpty() const
{
	return m_MouseBuffer.empty();
}

Mouse::RawDelta Mouse::ReadRawDelta()
{
	if (Mouse::RawDelta e;
		m_RawDeltaBuffer.pop(e, 0))
		return e;
	return { 0, 0 };
}

bool Mouse::RawDeltaBufferIsEmpty() const
{
	return m_RawDeltaBuffer.empty();
}

void Mouse::OnMouseMove(int x, int y)
{
	this->m_X = x;
	this->m_Y = y;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::Move, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMouseEnter()
{
	m_IsInWindow = true;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::Enter, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMouseLeave()
{
	m_IsInWindow = false;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::Leave, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnLMBPress(int x, int y)
{
	m_LeftMouseButtonIsPressed = true;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::LMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnLMBRelease(int x, int y)
{
	m_LeftMouseButtonIsPressed = false;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::LMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBPress(int x, int y)
{
	m_MiddleMouseButtonIsPressed = true;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::MMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBRelease(int x, int y)
{
	m_MiddleMouseButtonIsPressed = false;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::MMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBPress(int x, int y)
{
	m_RightMouseButtonIsPressed = true;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::RMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBRelease(int x, int y)
{
	m_RightMouseButtonIsPressed = false;
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::RMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelUp(int x, int y)
{
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::WheelUp, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDown(int x, int y)
{
	m_MouseBuffer.push(Mouse::Event(Mouse::Event::Type::WheelDown, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDelta(int x, int y, int delta)
{
	m_WheelDeltaCarry += delta;
	// generate events for every 120 
	while (m_WheelDeltaCarry >= WHEEL_DELTA)
	{
		m_WheelDeltaCarry -= WHEEL_DELTA;
		OnWheelUp(x, y);
	}
	while (m_WheelDeltaCarry <= -WHEEL_DELTA)
	{
		m_WheelDeltaCarry += WHEEL_DELTA;
		OnWheelDown(x, y);
	}
}

void Mouse::OnRawDelta(int dx, int dy)
{
	m_RawDeltaBuffer.push({ dx, dy });
	TrimBuffer(m_RawDeltaBuffer);
}