#include "pch.h"
#include "Window.h"
#include "Math/MathLibrary.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

Window::Window(LPCWSTR WindowName,
	int Width, int Height,
	int X, int Y)
{
	m_CursorEnabled = true;
	m_WindowHandle = nullptr;
	// Generate a unique name for our window class (This name has to be unique, Win32 API requirements)
	auto randomString = [](size_t length) -> std::wstring
	{
		auto randchar = []() -> char
		{
			const char charset[] = "0123456789" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz";
			const size_t max_index = (sizeof(charset) - 1);
			return charset[rand() % max_index];
		};
		std::wstring str(length, 0);
		std::generate_n(str.begin(), length, randchar);
		return str;
	};

	m_ClassName = L"KHWindowClass:" + randomString(9);

	// Register RID for handling input
	RAWINPUTDEVICE rid = {};
	rid.usUsagePage = 0x01; // mouse page
	rid.usUsage = 0x02l; // mouse usage
	rid.dwFlags = 0;
	rid.hwndTarget = nullptr;
	if (!::RegisterRawInputDevices(&rid, 1, sizeof(rid)))
	{
		CORE_ERROR("RegisterRawInputDevices Error: {}", ::GetLastError());
	}

	// Register window class
	HINSTANCE hInstance = GetModuleHandle(NULL);
	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = Window::WindowProcedure;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInstance;
	windowClass.hIcon = NULL;
	windowClass.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName = m_ClassName.data();
	windowClass.hIconSm = NULL;
	if (!RegisterClassExW(&windowClass))
	{
		CORE_ERROR("RegisterClassExW Error: {}", ::GetLastError());
	}

	// Create window
	m_WindowName = WindowName;
	m_WindowHandle = CreateWindowW(m_ClassName.data(), m_WindowName.data(),
		WS_OVERLAPPED | WS_MINIMIZEBOX | WS_SYSMENU,
		X, Y, Width, Height, NULL, NULL, hInstance, this);
	if (m_WindowHandle)
	{
		// Initialize ImGui for win32
		ImGui_ImplWin32_Init(m_WindowHandle);

		::ShowWindow(m_WindowHandle, SW_SHOW);
		::UpdateWindow(m_WindowHandle);
	}
	else
	{
		CORE_ERROR("CreateWindowW Error: {}", ::GetLastError());
	}
}

Window::~Window()
{
	::DestroyWindow(m_WindowHandle);
	m_WindowHandle = nullptr;
	::UnregisterClass(m_ClassName.data(), GetModuleHandle(NULL));
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
	if (ImGui_ImplWin32_WndProcHandler(m_WindowHandle, uMsg, wParam, lParam))
		return true;
	const ImGuiIO& imio = ImGui::GetIO();

	switch (uMsg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(m_WindowHandle, &ps);
		EndPaint(m_WindowHandle, &ps);
	}
	break;

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
		if (imio.WantCaptureMouse)
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
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		Mouse.OnLMBPress(pt.x, pt.y);
	}
	break;
	case WM_RBUTTONDOWN:
	{
		// Stifle this mouse message if imgui wants to capture
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		Mouse.OnRMBPress(pt.x, pt.y);
	}
	break;
	case WM_LBUTTONUP:
	{
		// Stifle this mouse message if imgui wants to capture
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		Mouse.OnLMBRelease(pt.x, pt.y);
		// Release mouse if outside of window
		if (pt.x < 0 || pt.x >= m_WindowWidth || pt.y < 0 || pt.y >= m_WindowHeight)
		{
			::ReleaseCapture();
			Mouse.OnMouseLeave();
		}
	}
	break;
	case WM_RBUTTONUP:
	{
		// Stifle this mouse message if imgui wants to capture
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		Mouse.OnRMBRelease(pt.x, pt.y);
		// Release mouse if outside of window
		if (pt.x < 0 || pt.x >= m_WindowWidth || pt.y < 0 || pt.y >= m_WindowHeight)
		{
			::ReleaseCapture();
			Mouse.OnMouseLeave();
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		// Stifle this mouse message if imgui wants to capture
		if (imio.WantCaptureMouse)
			break;
		const POINTS pt = MAKEPOINTS(lParam);
		const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		Mouse.OnWheelDelta(pt.x, pt.y, delta);
	}
	break;
#pragma endregion

#pragma region Raw Mouse Event
	case WM_INPUT:
	{
		if (!Mouse.RawInputEnabled())
			break;
		UINT size = 0;
		// First get the size of the input data
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) == -1)
		{
			CORE_ERROR("GetRawInputData Error: ", ::GetLastError());
			break;
		}
		BYTE* rawBuffer = (BYTE*)alloca(size);
		// Read in the input data
		if (::GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, rawBuffer, &size, sizeof(RAWINPUTHEADER)) != size)
		{
			CORE_ERROR("The number of bytes copied into pData does not match the size queried");
			break;
		}
		// Process the raw input data
		const RAWINPUT& rawInput = reinterpret_cast<const RAWINPUT&>(*rawBuffer);
		if (rawInput.header.dwType == RIM_TYPEMOUSE &&
			(rawInput.data.mouse.lLastX != 0 || rawInput.data.mouse.lLastY != 0))
		{
			Mouse.OnRawDelta(rawInput.data.mouse.lLastX, rawInput.data.mouse.lLastY);
		}
	}
	break;
#pragma endregion

#pragma region Keyboard Event
	case WM_SYSCHAR: {} break;

	case WM_KILLFOCUS:
	{
		Keyboard.ResetKeyState();
	}
	break;

	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (imio.WantCaptureKeyboard)
			break;
		if (!(lParam & (1 << 30)) || Keyboard.AutoRepeatEnabled())
			Keyboard.OnKeyPress(static_cast<unsigned char>(wParam));
	}
	break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (imio.WantCaptureKeyboard)
			break;
		Keyboard.OnKeyRelease(static_cast<unsigned char>(wParam));
	}
	break;

	case WM_CHAR:
	{
		// Stifle this keyboard message if imgui wants to capture
		if (imio.WantCaptureKeyboard)
			break;
		Keyboard.OnChar(static_cast<unsigned char>(wParam));
	}
	break;
#pragma endregion

#pragma region Resize Message
	case WM_SIZE:
	case WM_ENTERSIZEMOVE: // WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_EXITSIZEMOVE: // WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	{
		RECT clientRect = {};
		::GetWindowRect(m_WindowHandle, &clientRect);
		m_WindowWidth = Math::Max<unsigned int>(1u, clientRect.right - clientRect.left);
		m_WindowHeight = Math::Max<unsigned int>(1u, clientRect.bottom - clientRect.top);

		Window::Message windowMessage;
		windowMessage.type = Message::Type::Resize;
		windowMessage.data.Width = m_WindowWidth;
		windowMessage.data.Height = m_WindowHeight;
		MessageQueue.push(windowMessage);
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
	Window* self;
	if (uMsg == WM_NCCREATE)
	{
		LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
		self = reinterpret_cast<Window*>(lpcs->lpCreateParams);
		self->m_WindowHandle = hwnd;
		::SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(self));
	}
	else
	{
		self = reinterpret_cast<Window*>(::GetWindowLongPtr(hwnd, GWLP_USERDATA));
	}
	if (self)
	{
		return self->DispatchEvent(uMsg, wParam, lParam);
	}
	else
	{
		return ::DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}

void Window::ConfineCursor()
{
	RECT rect;
	::GetClientRect(m_WindowHandle, &rect);
	::MapWindowPoints(m_WindowHandle, nullptr, reinterpret_cast<POINT*>(&rect), 2);
	::ClipCursor(&rect);
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