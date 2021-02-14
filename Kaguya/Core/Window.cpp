#include "pch.h"
#include "Window.h"

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
	int X /*= CW_USEDEFAULT*/, int Y /*= CW_USEDEFAULT*/,
	bool Maximize /*= false*/)
{
	m_WindowName = WindowName;
	m_WindowWidth = Width;
	m_WindowHeight = Height;
	m_CursorEnabled = true;

	// Register window class
	WNDCLASSEXW WndClassExW = {};
	WndClassExW.cbSize = sizeof(WNDCLASSEX);
	WndClassExW.style = CS_HREDRAW | CS_VREDRAW;
	WndClassExW.lpfnWndProc = Window::WindowProcedure;
	WndClassExW.cbClsExtra = 0;
	WndClassExW.cbWndExtra = 0;
	WndClassExW.hInstance = GetModuleHandle(NULL);
	WndClassExW.hIcon = m_Icon.get();
	WndClassExW.hCursor = m_Cursor.get();
	WndClassExW.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	WndClassExW.lpszMenuName = NULL;
	WndClassExW.lpszClassName = WindowName;
	WndClassExW.hIconSm = m_Icon.get();
	if (!RegisterClassExW(&WndClassExW))
	{
		LOG_ERROR("RegisterClassExW Error: {}", ::GetLastError());
	}

	// Create window
	DWORD dwStyle = WS_OVERLAPPEDWINDOW;
	dwStyle |= Maximize ? WS_MAXIMIZE : 0;
	m_WindowHandle = wil::unique_hwnd(::CreateWindowW(WindowName, WindowName,
		dwStyle, X, Y, Width, Height, NULL, NULL, GetModuleHandle(NULL), NULL));
	if (m_WindowHandle)
	{
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

void Window::Render()
{
	if (m_RenderFunc)
	{
		m_RenderFunc();
	}
}

void Window::Resize(UINT Width, UINT Height)
{
	if (m_ResizeFunc)
	{
		m_ResizeFunc(Width, Height);
	}
}

void Window::SetRenderFunc(std::function<void()> RenderFunc)
{
	m_RenderFunc = std::move(RenderFunc);
}

void Window::SetResizeFunc(ResizeFunc ResizeFunc)
{
	m_ResizeFunc = std::move(ResizeFunc);
}

LRESULT Window::DispatchEvent(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(m_WindowHandle.get(), uMsg, wParam, lParam))
	{
		return true;
	}

	switch (uMsg)
	{
	case WM_SIZE:
	{
		if (wParam == SIZE_MINIMIZED)
		{
			break;
		}

		RECT Rect = {};
		::GetClientRect(m_WindowHandle.get(), &Rect);
		m_WindowWidth = Rect.right - Rect.left;
		m_WindowHeight = Rect.bottom - Rect.top;

		Window::Message WindowMessage = {};
		WindowMessage.Type = Message::EType::Resize;
		WindowMessage.Data.Width = m_WindowWidth;
		WindowMessage.Data.Height = m_WindowHeight;
		MessageQueue.Enqueue(WindowMessage);
	}
	break;

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

	// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
	{
		auto pMinMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);

		pMinMaxInfo->ptMinTrackSize.x = Application::MinimumWidth;
		pMinMaxInfo->ptMinTrackSize.y = Application::MinimumHeight;
	}
	break;

	case WM_DESTROY:
	{
		::PostQuitMessage(0);
	}
	break;

	default:
		return ::DefWindowProc(m_WindowHandle.get(), uMsg, wParam, lParam);
	}

	return 0;
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