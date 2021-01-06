#pragma once

#include "Window.h"

class InputHandler
{
public:
	InputHandler(Window* pWindow)
		: pWindow(pWindow)
	{

	}

	void Handle(const MSG* pMsg)
	{
		if (!pWindow->IsFocused())
		{
			return;
		}

		switch (pMsg->message)
		{
		case WM_INPUT:
		{
			UINT dwSize = 0;
			::GetRawInputData(reinterpret_cast<HRAWINPUT>(pMsg->lParam), RID_INPUT, NULL, &dwSize,
				sizeof(RAWINPUTHEADER));

			LPBYTE lpb = (LPBYTE)_malloca(dwSize);
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
			if (Points.x >= 0 && Points.x < pWindow->GetWindowWidth() && Points.y >= 0 && Points.y < pWindow->GetWindowHeight())
			{
				Mouse.OnMouseMove(Points.x, Points.y);
				if (!Mouse.IsInWindow())
				{
					::SetCapture(pWindow->GetWindowHandle());
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

private:
	Window* pWindow;
	Mouse Mouse;
	Keyboard Keyboard;
};