#include "pch.h"
#include "Mouse.h"

Mouse::Mouse()
{
	m_X = m_Y = 0;
	m_IsLMBPressed = m_IsMMBPressed = m_IsRMBPressed = m_IsInWindow = false;
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

bool Mouse::IsLMBPressed() const
{
	return m_IsLMBPressed;
}

bool Mouse::IsMMBPressed() const
{
	return m_IsMMBPressed;
}

bool Mouse::IsRMBPressed() const
{
	return m_IsRMBPressed;
}

bool Mouse::IsInWindow() const
{
	return m_IsInWindow;
}

Mouse::Event Mouse::Read()
{
	if (Mouse::Event e;
		m_MouseBuffer.Dequeue(e, 0))
		return e;
	return Mouse::Event(Mouse::Event::EType::Invalid, *this);
}

bool Mouse::MouseBufferIsEmpty() const
{
	return m_MouseBuffer.IsEmpty();
}

Mouse::RawDelta Mouse::ReadRawDelta()
{
	if (Mouse::RawDelta e;
		m_RawDeltaBuffer.Dequeue(e, 0))
		return e;
	return { 0, 0 };
}

bool Mouse::RawDeltaBufferIsEmpty() const
{
	return m_RawDeltaBuffer.IsEmpty();
}

void Mouse::OnMouseMove(int x, int y)
{
	this->m_X = x;
	this->m_Y = y;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::Move, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMouseEnter()
{
	m_IsInWindow = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::Enter, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMouseLeave()
{
	m_IsInWindow = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::Leave, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnLMBPress(int x, int y)
{
	m_IsLMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnLMBRelease(int x, int y)
{
	m_IsLMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBPress(int x, int y)
{
	m_IsMMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBRelease(int x, int y)
{
	m_IsMMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBPress(int x, int y)
{
	m_IsRMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBRelease(int x, int y)
{
	m_IsRMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelUp(int x, int y)
{
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::WheelUp, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDown(int x, int y)
{
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::WheelDown, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDelta(int x, int y, int delta)
{
	m_WheelDeltaCarry += delta;
	// Generate events for every 120 
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
	m_RawDeltaBuffer.Enqueue({ dx, dy });
	TrimBuffer(m_RawDeltaBuffer);
}