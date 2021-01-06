#include "pch.h"
#include "InputHandler.h"

#include <hidusage.h>

#include "Window.h"

void InputHandler::Create(Window* pWindow)
{
	m_pWindow = pWindow;

	// Register RID for handling input
	RAWINPUTDEVICE RID[2] = {};

	RID[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RID[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	RID[0].dwFlags = RIDEV_INPUTSINK;
	RID[0].hwndTarget = pWindow->GetWindowHandle();

	RID[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
	RID[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
	RID[1].dwFlags = RIDEV_INPUTSINK;
	RID[1].hwndTarget = pWindow->GetWindowHandle();

	if (!::RegisterRawInputDevices(RID, 2, sizeof(RID[0])))
	{
		LOG_ERROR("RegisterRawInputDevices Error: {}", ::GetLastError());
	}
}

void InputHandler::Handle(const MSG* pMsg)
{
	if (!m_pWindow->IsFocused())
	{
		return;
	}

	switch (pMsg->message)
	{
	case WM_KILLFOCUS:
	{
		Keyboard.ResetKeyState();
	}
	break;

	case WM_INPUT:
	{
		UINT dwSize = 0;
		::GetRawInputData(reinterpret_cast<HRAWINPUT>(pMsg->lParam), RID_INPUT, NULL, &dwSize,
			sizeof(RAWINPUTHEADER));

		auto lpb = (LPBYTE)_malloca(dwSize);
		if (!lpb)
		{
			return;
		}

		::GetRawInputData(reinterpret_cast<HRAWINPUT>(pMsg->lParam), RID_INPUT, lpb, &dwSize,
			sizeof(RAWINPUTHEADER));

		auto pRawInput = reinterpret_cast<RAWINPUT*>(lpb);

		switch (pRawInput->header.dwType)
		{
		case RIM_TYPEMOUSE:
		{
			const auto& mouse = pRawInput->data.mouse;

			if ((mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN))		Mouse.OnLMBPress(mouse.lLastX, mouse.lLastY);
			if ((mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN))	Mouse.OnMMBPress(mouse.lLastX, mouse.lLastY);
			if ((mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN))		Mouse.OnRMBPress(mouse.lLastX, mouse.lLastY);

			if ((mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP))		Mouse.OnLMBRelease(mouse.lLastX, mouse.lLastY);
			if ((mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP))		Mouse.OnMMBRelease(mouse.lLastX, mouse.lLastY);
			if ((mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP))		Mouse.OnRMBRelease(mouse.lLastX, mouse.lLastY);

			Mouse.OnRawDelta(mouse.lLastX, mouse.lLastY);
		}
		break;

		case RIM_TYPEKEYBOARD:
		{
			const auto& keyboard = pRawInput->data.keyboard;

			if (keyboard.Message == WM_KEYDOWN || keyboard.Message == WM_SYSKEYDOWN)
			{
				Keyboard.OnKeyPress(keyboard.VKey);
			}

			if (keyboard.Message == WM_KEYUP || keyboard.Message == WM_SYSKEYUP)
			{
				Keyboard.OnKeyRelease(keyboard.VKey);
			}
		}
		break;
		}

		_freea(lpb);
	}
	break;

	case WM_MOUSEMOVE:
	{
		const POINTS Points = MAKEPOINTS(pMsg->lParam);
		// Within the range of our window dimension -> log move, and log enter + capture mouse (if not previously in window)
		if (Points.x >= 0 && Points.x < m_pWindow->GetWindowWidth() && Points.y >= 0 && Points.y < m_pWindow->GetWindowHeight())
		{
			Mouse.OnMouseMove(Points.x, Points.y);
			if (!Mouse.IsInWindow())
			{
				::SetCapture(m_pWindow->GetWindowHandle());
				Mouse.OnMouseEnter();
			}
		}
		// Outside the range of our window dimension -> log move / maintain capture if button down
		else
		{
			if (pMsg->wParam & (MK_LBUTTON | MK_RBUTTON))
			{
				Mouse.OnMouseMove(Points.x, Points.y);
			}
			// button up -> release capture / log event for leaving
			else
			{
				::ReleaseCapture();
				Mouse.OnMouseLeave();
			}
		}
	}
	break;
	}
}