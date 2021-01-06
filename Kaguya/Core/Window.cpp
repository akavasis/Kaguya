#include "pch.h"
#include "Window.h"

#include <hidusage.h>

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::~Window()
{
	if (!m_WindowName.empty())
	{
		::UnregisterClass(m_WindowName.data(), GetModuleHandle(NULL));
	}
}

void Window::SetIcon(HANDLE Image)
{
	m_Icon.reset(reinterpret_cast<HICON>(Image));
}

void Window::SetCursor(HCURSOR Cursor)
{
	m_Cursor.reset(Cursor);
}

void Window::Create(LPCWSTR WindowName,
	int Width /*= CW_USEDEFAULT*/, int Height /*= CW_USEDEFAULT*/,
	int X /*= CW_USEDEFAULT*/, int Y /*= CW_USEDEFAULT*/)
{
	m_WindowName	= WindowName;
	m_WindowWidth	= Width;
	m_WindowHeight	= Height;
	m_CursorEnabled = true;

	// Register window class
	WNDCLASSEXW windowClass		= {};
	windowClass.cbSize			= sizeof(WNDCLASSEX);
	windowClass.style			= CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc		= Window::WindowProcedure;
	windowClass.cbClsExtra		= 0;
	windowClass.cbWndExtra		= 0;
	windowClass.hInstance		= GetModuleHandle(NULL);
	windowClass.hIcon			= m_Icon.get();
	windowClass.hCursor			= m_Cursor.get();
	windowClass.hbrBackground	= reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszMenuName	= NULL;
	windowClass.lpszClassName	= WindowName;
	windowClass.hIconSm			= m_Icon.get();
	if (!RegisterClassExW(&windowClass))
	{
		LOG_ERROR("RegisterClassExW Error: {}", ::GetLastError());
	}

	// Create window
	m_WindowHandle = wil::unique_hwnd(::CreateWindowW(WindowName, WindowName,
		WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU,
		X, Y, Width, Height, NULL, NULL, GetModuleHandle(NULL), NULL));
	if (m_WindowHandle)
	{
		// Register RID for handling input
		RAWINPUTDEVICE RID[1]	= {};
		RID[0].usUsagePage		= HID_USAGE_PAGE_GENERIC;
		RID[0].usUsage			= HID_USAGE_GENERIC_MOUSE;
		RID[0].dwFlags			= RIDEV_INPUTSINK;
		RID[0].hwndTarget		= m_WindowHandle.get();
		if (!::RegisterRawInputDevices(RID, 1, sizeof(RID[0])))
		{
			LOG_ERROR("RegisterRawInputDevices Error: {}", ::GetLastError());
		}

		// Initialize ImGui for win32
		ImGui_ImplWin32_Init(m_WindowHandle.get());
		::SetWindowLongPtr(m_WindowHandle.get(), GWLP_USERDATA, reinterpret_cast<LPARAM>(this));

		::ShowWindow(m_WindowHandle.get(), SW_SHOW);
		::UpdateWindow(m_WindowHandle.get());
	}
	else
	{
		LOG_ERROR("CreateWindowW Error: {}", ::GetLastError());
	}
}

void Window::EnableCursor()
{
	m_CursorEnabled = true;
	ShowCursor();
	ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
	FreeCursor();
}

void Window::DisableCursor()
{
	m_CursorEnabled = false;
	HideCursor();
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouse;
	ConfineCursor();
}

bool Window::CursorEnabled() const
{
	return m_CursorEnabled;
}

bool Window::IsFocused() const
{
	return GetForegroundWindow() == m_WindowHandle.get();
}

void Window::SetTitle(const std::wstring& Title)
{
	::SetWindowText(m_WindowHandle.get(), Title.data());
}

void Window::AppendToTitle(const std::wstring& Message)
{
	::SetWindowText(m_WindowHandle.get(), (m_WindowName + Message).data());
}

LRESULT Window::DispatchEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const ImGuiIO* pImGuiIO = nullptr;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		pImGuiIO = &ImGui::GetIO();
		if (ImGui_ImplWin32_WndProcHandler(m_WindowHandle.get(), uMsg, wParam, lParam))
			return true;
	}

	switch (uMsg)
	{
#pragma region Mouse Event
	case WM_MOUSEMOVE:
	{
		if (!m_CursorEnabled)
		{
			if (!Mouse.IsInWindow())
			{
				::SetCapture(m_WindowHandle.get());
				Mouse.OnMouseEnter();
				HideCursor();
			}
			break;
		}

		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		// Within the range of our window dimension -> log move, and log enter + capture mouse (if not previously in window)
		if (Points.x >= 0 && Points.x < m_WindowWidth && Points.y >= 0 && Points.y < m_WindowHeight)
		{
			Mouse.OnMouseMove(Points.x, Points.y);
			if (!Mouse.IsInWindow())
			{
				::SetCapture(m_WindowHandle.get());
				Mouse.OnMouseEnter();
			}
		}
		// Outside the range of our window dimension -> log move / maintain capture if button down
		else
		{
			if (wParam & (MK_LBUTTON | MK_RBUTTON))
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

	case WM_LBUTTONDOWN:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnLMBPress(Points.x, Points.y);
	}
	break;

	case WM_RBUTTONDOWN:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnRMBPress(Points.x, Points.y);
	}
	break;

	case WM_LBUTTONUP:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnLMBRelease(Points.x, Points.y);
		// Release mouse if outside of window
		if (Points.x < 0 || Points.x >= m_WindowWidth || Points.y < 0 || Points.y >= m_WindowHeight)
		{
			::ReleaseCapture();
			Mouse.OnMouseLeave();
		}
	}
	break;

	case WM_RBUTTONUP:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnRMBRelease(Points.x, Points.y);
		// Release mouse if outside of window
		if (Points.x < 0 || Points.x >= m_WindowWidth || Points.y < 0 || Points.y >= m_WindowHeight)
		{
			::ReleaseCapture();
			Mouse.OnMouseLeave();
		}
	}
	break;

	case WM_MOUSEWHEEL:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
		{
			break;
		}

		const POINTS Points = MAKEPOINTS(lParam);
		const int WheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
		Mouse.OnWheelDelta(Points.x, Points.y, WheelDelta);
	}
	break;
#pragma endregion

#pragma region Raw Mouse Event
	case WM_INPUT:
	{
		if (!Mouse.RawInputEnabled())
			break;

		UINT dwSize = sizeof(RAWINPUT);
		static BYTE lpb[sizeof(RAWINPUT)] = {};

		GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));

		auto pRawInput = (RAWINPUT*)lpb;

		if (pRawInput->header.dwType == RIM_TYPEMOUSE)
		{
			int xPosRelative = pRawInput->data.mouse.lLastX;
			int yPosRelative = pRawInput->data.mouse.lLastY;
			Mouse.OnRawDelta(xPosRelative, yPosRelative);
		}
	}
	break;
#pragma endregion

#pragma region Keyboard Event
	case WM_SYSCHAR:
	{
	}
	break;

	// Clear key state to avoid zombie keys when main window is out of focus
	case WM_KILLFOCUS:
	{
		Keyboard.ResetKeyState();
	}
	break;

	case WM_KEYDOWN: [[fallthrough]];
	case WM_SYSKEYDOWN:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
		{
			break;
		}
		
		if (!(lParam & (1 << 30)) || Keyboard.AutoRepeat)
		{
			Keyboard.OnKeyPress(static_cast<unsigned char>(wParam));
		}
	}
	break;

	case WM_KEYUP: [[fallthrough]];
	case WM_SYSKEYUP:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
		{
			break;
		}
		
		Keyboard.OnKeyRelease(static_cast<unsigned char>(wParam));
	}
	break;

	case WM_CHAR:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
		{
			break;
		}
		
		Keyboard.OnChar(static_cast<unsigned char>(wParam));
	}
	break;
#pragma endregion

#pragma region Resize Message
	case WM_SIZE:
	case WM_ENTERSIZEMOVE:	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_EXITSIZEMOVE:	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	{
		RECT WindowRect = {};
		::GetWindowRect(m_WindowHandle.get(), &WindowRect);
		m_WindowWidth	= WindowRect.right - WindowRect.left;
		m_WindowHeight	= WindowRect.bottom - WindowRect.top;

		Window::Message WindowMessage	= {};
		WindowMessage.Type				= Message::EType::Resize;
		WindowMessage.Data.Width		= m_WindowWidth;
		WindowMessage.Data.Height		= m_WindowHeight;
		MessageQueue.Enqueue(WindowMessage);
	}
	break;
#pragma endregion

	case WM_ACTIVATE:
	{
		// Confine/free cursor on window to foreground/background if cursor disabled
		if (!m_CursorEnabled)
		{
			if (wParam & WA_ACTIVE)
			{
				ConfineCursor();
				HideCursor();
			}
			else
			{
				FreeCursor();
				ShowCursor();
			}
		}
	}
	break;

	case WM_DESTROY:
	{
		::PostQuitMessage(0);
	}
	break;
	}

	return ::DefWindowProc(m_WindowHandle.get(), uMsg, wParam, lParam);
}

LRESULT CALLBACK Window::WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	auto pWindow = reinterpret_cast<Window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	if (pWindow)
	{
		return pWindow->DispatchEvent(uMsg, wParam, lParam);
	}
	return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void Window::ConfineCursor()
{
	RECT ClientRect = {};
	::GetClientRect(m_WindowHandle.get(), &ClientRect);
	::MapWindowPoints(m_WindowHandle.get(), nullptr, reinterpret_cast<POINT*>(&ClientRect), 2);
	::ClipCursor(&ClientRect);
}

void Window::FreeCursor()
{
	::ClipCursor(nullptr);
}

void Window::ShowCursor()
{
	while (::ShowCursor(TRUE) < 0);
}

void Window::HideCursor()
{
	while (::ShowCursor(FALSE) >= 0);
}

Window::ImGuiContextManager::ImGuiContextManager()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
}

Window::ImGuiContextManager::~ImGuiContextManager()
{
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}