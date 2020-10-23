#include "pch.h"
#include "Window.h"
#include "Application.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window(LPCWSTR WindowName,
	int Width /*= CW_USEDEFAULT*/, int Height /*= CW_USEDEFAULT*/,
	int X /*= CW_USEDEFAULT*/, int Y /*= CW_USEDEFAULT*/)
	: m_WindowName(WindowName),
	m_WindowWidth(Width),
	m_WindowHeight(Height)
{
	m_Icon = (HICON)::LoadImage(0, (Application::ExecutableFolderPath / "Assets/Kaguya.ico").generic_wstring().data(), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_LOADFROMFILE);
	m_Cursor = ::LoadCursor(nullptr, IDC_ARROW);

	m_CursorEnabled = true;

	// Register RID for handling input
	RAWINPUTDEVICE rid	= {};
	rid.usUsagePage		= 0x01;		// mouse page
	rid.usUsage			= 0x02l;	// mouse usage
	rid.dwFlags			= 0;
	rid.hwndTarget		= NULL;
	if (!::RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		LOG_ERROR("RegisterRawInputDevices Error: {}", ::GetLastError());
	}

	// Register window class
	WNDCLASSEXW windowClass		= {};
	windowClass.cbSize			= sizeof(WNDCLASSEX);
	windowClass.style			= CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc		= Window::WindowProcedure;
	windowClass.cbClsExtra		= 0;
	windowClass.cbWndExtra		= 0;
	windowClass.hInstance		= GetModuleHandle(NULL);
	windowClass.hIcon			= m_Icon;
	windowClass.hCursor			= m_Cursor;
	windowClass.hbrBackground	= reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszMenuName	= NULL;
	windowClass.lpszClassName	= WindowName;
	windowClass.hIconSm			= m_Icon;
	if (!RegisterClassExW(&windowClass))
	{
		LOG_ERROR("RegisterClassExW Error: {}", ::GetLastError());
	}

	// Create window
	m_WindowHandle = CreateWindowW(WindowName, WindowName,
		WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU,
		X, Y, Width, Height, NULL, NULL, GetModuleHandle(NULL), this);
	if (m_WindowHandle)
	{
		// Initialize ImGui for win32
		ImGui_ImplWin32_Init(m_WindowHandle);

		::ShowWindow(m_WindowHandle, SW_SHOW);
		::UpdateWindow(m_WindowHandle);
	}
	else
	{
		LOG_ERROR("CreateWindowW Error: {}", ::GetLastError());
	}
}

Window::~Window()
{
	::DestroyWindow(m_WindowHandle);
	::UnregisterClass(m_WindowName.data(), GetModuleHandle(NULL));

	DestroyCursor(m_Cursor);
	DestroyIcon(m_Icon);
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
	return GetForegroundWindow() == m_WindowHandle;
}

void Window::SetTitle(std::wstring Title)
{
	::SetWindowText(m_WindowHandle, Title.data());
}

void Window::AppendToTitle(std::wstring Message)
{
	::SetWindowText(m_WindowHandle, (m_WindowName + Message).data());
}

LRESULT Window::DispatchEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const ImGuiIO* pImGuiIO = nullptr;
	if (ImGui::GetCurrentContext() != nullptr)
	{
		pImGuiIO = &ImGui::GetIO();
		if (ImGui_ImplWin32_WndProcHandler(m_WindowHandle, uMsg, wParam, lParam))
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
				::SetCapture(m_WindowHandle);
				Mouse.OnMouseEnter();
				HideCursor();
			}
			break;
		}
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		// Within the range of our window dimension -> log move, and log enter + capture mouse (if not previously in window)
		if (pt.x >= 0 && pt.x < m_WindowWidth && pt.y >= 0 && pt.y < m_WindowHeight)
		{
			Mouse.OnMouseMove(pt.x, pt.y);
			if (!Mouse.IsInWindow())
			{
				::SetCapture(m_WindowHandle);
				Mouse.OnMouseEnter();
			}
		}
		// Outside the range of our window dimension -> log move / maintain capture if button down
		else
		{
			if (wParam & (MK_LBUTTON | MK_RBUTTON))
			{
				Mouse.OnMouseMove(pt.x, pt.y);
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
		::SetForegroundWindow(m_WindowHandle);
		if (!m_CursorEnabled)
		{
			ConfineCursor();
			HideCursor();
		}
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
			break;
		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnLMBPress(Points.x, Points.y);
	}
	break;
	case WM_RBUTTONDOWN:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
			break;
		const POINTS Points = MAKEPOINTS(lParam);
		Mouse.OnRMBPress(Points.x, Points.y);
	}
	break;
	case WM_LBUTTONUP:
	{
		// Stifle this mouse message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureMouse)
			break;
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
			break;
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
			break;
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
		UINT DataSize = 0;
		// First get the size of the input data
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &DataSize, sizeof(RAWINPUTHEADER)) == -1)
			break;
		BYTE* pData = (BYTE*)_malloca(DataSize);
		// Read in the input data
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, pData, &DataSize, sizeof(RAWINPUTHEADER)) != DataSize)
			break;
		// Process the raw input data
		const RAWINPUT& RawInput = reinterpret_cast<const RAWINPUT&>(*pData);
		if (RawInput.header.dwType == RIM_TYPEMOUSE &&
			(RawInput.data.mouse.lLastX != 0 || RawInput.data.mouse.lLastY != 0))
		{
			Mouse.OnRawDelta(RawInput.data.mouse.lLastX, RawInput.data.mouse.lLastY);
		}
		_freea(pData);
	}
	break;
#pragma endregion

#pragma region Keyboard Event
	case WM_SYSCHAR:
	{
	}
	break;

	case WM_KILLFOCUS:
	{
		Keyboard.ResetKeyState();
	}
	break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
			break;
		if (!(lParam & (1 << 30)) || Keyboard.AutoRepeatEnabled())
			Keyboard.OnKeyPress(static_cast<unsigned char>(wParam));
	}
	break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
			break;
		Keyboard.OnKeyRelease(static_cast<unsigned char>(wParam));
	}
	break;

	case WM_CHAR:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (pImGuiIO && pImGuiIO->WantCaptureKeyboard)
			break;
		Keyboard.OnChar(static_cast<unsigned char>(wParam));
	}
	break;
#pragma endregion

#pragma region Resize Message
	case WM_SIZE:
	case WM_ENTERSIZEMOVE:	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_EXITSIZEMOVE:	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	{
		RECT WindowRect = {}; ::GetWindowRect(m_WindowHandle, &WindowRect);
		m_WindowWidth	= Math::Max<unsigned int>(1u, WindowRect.right - WindowRect.left);
		m_WindowHeight	= Math::Max<unsigned int>(1u, WindowRect.bottom - WindowRect.top);

		Window::Message WindowMessage;
		WindowMessage.type			= Message::Type::Resize;
		WindowMessage.data.Width	= m_WindowWidth;
		WindowMessage.data.Height	= m_WindowHeight;
		MessageQueue.push(WindowMessage);
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

	return ::DefWindowProc(m_WindowHandle, uMsg, wParam, lParam);
}

LRESULT CALLBACK Window::WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	Window* pWindow = nullptr;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		pWindow = reinterpret_cast<Window*>(lpcs->lpCreateParams);
		pWindow->m_WindowHandle = hwnd;
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(pWindow));
	}
	else
	{
		pWindow = reinterpret_cast<Window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	if (pWindow)
	{
		return pWindow->DispatchEvent(uMsg, wParam, lParam);
	}
	else
	{
		return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

void Window::ConfineCursor()
{
	RECT ClientRect = {}; ::GetClientRect(m_WindowHandle, &ClientRect);
	::MapWindowPoints(m_WindowHandle, nullptr, reinterpret_cast<POINT*>(&ClientRect), 2);
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