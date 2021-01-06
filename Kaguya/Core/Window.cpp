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