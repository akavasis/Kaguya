#include "pch.h"
#include "Mouse.h"

Mouse::Mouse()
{
	for (auto& Button : m_ButtonStates)
	{
		Button = false;
	}
	m_WheelDeltaCarry = 0;
}

bool Mouse::IsMouseButtonPressed(Button Button) const
{
	return m_ButtonStates[Button];
}

bool Mouse::IsLMBPressed() const
{
	return IsMouseButtonPressed(Button::Left);
}

bool Mouse::IsMMBPressed() const
{
	return IsMouseButtonPressed(Button::Middle);
}

bool Mouse::IsRMBPressed() const
{
	return IsMouseButtonPressed(Button::Right);
}

bool Mouse::IsInWindow() const
{
	return m_IsInWindow;
}

std::optional<Mouse::Event> Mouse::Read()
{
	if (Mouse::Event e; m_MouseBuffer.Dequeue(e, 0))
	{
		return e;
	}
	return {};
}

std::optional<Mouse::RawInput> Mouse::ReadRawInput()
{
	if (Mouse::RawInput e; m_RawDeltaBuffer.Dequeue(e, 0))
	{
		return e;
	}
	return {};
}

void Mouse::OnMouseMove(int X, int Y)
{
	this->X = X, this->Y = Y;
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

void Mouse::OnMouseButtonDown(Button Button)
{
	m_ButtonStates[Button] = true;
	switch (Button)
	{
	case Mouse::Left: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBPress, *this)); break;
	case Mouse::Middle: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBPress, *this)); break;
	case Mouse::Right: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBPress, *this)); break;
	case Mouse::NumButtons: [[fallthrough]];
	default:
		break;
	}
	TrimBuffer(m_MouseBuffer);
}

void Mouse::OnMouseButtonUp(Button Button)
{
	m_ButtonStates[Button] = false;
	switch (Button)
	{
	case Mouse::Left: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::LMBRelease, *this)); break;
	case Mouse::Middle: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::MMBRelease, *this)); break;
	case Mouse::Right: m_MouseBuffer.Enqueue(Mouse::Event(Mouse::Event::EType::RMBRelease, *this)); break;
	case Mouse::NumButtons: [[fallthrough]];
	default:
		break;
	}
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
	RawX = X, RawY = Y;
	m_RawDeltaBuffer.Enqueue({ X, Y });
	TrimBuffer(m_RawDeltaBuffer);
}