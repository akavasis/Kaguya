#include "pch.h"
#include "Mouse.h"

Mouse::Mouse()
{
	m_IsLMBPressed = m_IsMMBPressed = m_IsRMBPressed = m_IsInWindow = false;
	m_WheelDeltaCarry = 0;
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

std::optional<Mouse::Event> Mouse::Read()
{
	if (Mouse::Event e;
		m_MouseBuffer.Dequeue(e, 0))
		return e;
	return {};
}

std::optional<Mouse::RawInput> Mouse::ReadRawInput()
{
	if (Mouse::RawInput e;
		m_RawDeltaBuffer.Dequeue(e, 0))
		return e;
	return {};
}

void Mouse::OnMouseMove(int X, int Y)
{
	this->X = X;
	this->Y = Y;
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

void Mouse::OnLMBDown()
{
	m_IsLMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBDown()
{
	m_IsMMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBDown()
{
	m_IsRMBPressed = true;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBPress, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnLMBUp()
{
	m_IsLMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMMBUp()
{
	m_IsMMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnRMBUp()
{
	m_IsRMBPressed = false;
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBRelease, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDown()
{
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::WheelDown, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelUp()
{
	m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::WheelUp, *this));
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnWheelDelta(int WheelDelta)
{
	m_WheelDeltaCarry += WheelDelta;
	// Generate events for every 120 
	while (m_WheelDeltaCarry >= WHEEL_DELTA)
	{
		m_WheelDeltaCarry -= WHEEL_DELTA;
		OnWheelUp();
	}
	while (m_WheelDeltaCarry <= -WHEEL_DELTA)
	{
		m_WheelDeltaCarry += WHEEL_DELTA;
		OnWheelDown();
	}
}

void Mouse::OnRawInput(int X, int Y)
{
	RawX = X;
	RawY = Y;
	m_RawDeltaBuffer.Enqueue({ X, Y });
	TrimBuffer(m_RawDeltaBuffer);
}