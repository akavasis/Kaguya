#pragma once
#include <Windows.h>
#include <wil/resource.h>
#include <string>
#include "Mouse.h"
#include "Keyboard.h"
#include "ThreadSafeQueue.h"

class Window
{
public:
	struct Message
	{
		enum class Type
		{
			None,
			Resize
		} type;

		struct Data
		{
			UINT Width, Height;
		} data;

		Message() = default;
		Message(Type Type, Data Data)
			: type(Type), data(Data)
		{
		}
	};

	Window(LPCWSTR WindowName,
		int Width = CW_USEDEFAULT, int Height = CW_USEDEFAULT,
		int X = CW_USEDEFAULT, int Y = CW_USEDEFAULT);
	~Window();

	void EnableCursor();
	void DisableCursor();
	bool CursorEnabled() const;
	bool IsFocused() const;

	inline auto GetWindowHandle() const { return m_WindowHandle.get(); }
	inline auto GetWindowName() const { return m_WindowName; }
	inline auto GetWindowWidth() const { return m_WindowWidth; }
	inline auto GetWindowHeight() const { return m_WindowHeight; }

	void SetTitle(std::wstring Title);
	void AppendToTitle(std::wstring Message);

	Mouse						Mouse;
	Keyboard					Keyboard;
	ThreadSafeQueue<Message>	MessageQueue;
private:
	LRESULT DispatchEvent(UINT uMsg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

	void ConfineCursor();
	void FreeCursor();
	void ShowCursor();
	void HideCursor();

	class ImGuiContextManager
	{
	public:
		ImGuiContextManager();
		~ImGuiContextManager();
	};

	wil::unique_hicon			m_Icon;
	wil::unique_hcursor			m_Cursor;
	wil::unique_hwnd			m_WindowHandle;

	std::wstring				m_WindowName;
	unsigned int				m_WindowWidth;
	unsigned int				m_WindowHeight;
	bool						m_CursorEnabled;

	ImGuiContextManager			m_ImGuiContextManager;
};