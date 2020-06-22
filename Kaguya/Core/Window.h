#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string>
#include "EventDispatcher.h"
#include "Mouse.h"
#include "Keyboard.h"

class Window : public EventDispatcher
{
public:
	struct Event
	{
		enum class Type
		{
			Invalid,
			Minimize,
			Maximize,
			Resizing,
			Resize,
			Destroy
		} type;

		struct Data
		{
		} data;
	};

	Window(LPCWSTR WindowName,
		int Width = CW_USEDEFAULT, int Height = CW_USEDEFAULT,
		int X = CW_USEDEFAULT, int Y = CW_USEDEFAULT);
	~Window();

	void EnableCursor();
	void DisableCursor();
	bool CursorEnabled() const;

	inline const HWND& GetWindowHandle() const { return m_WindowHandle; }
	inline const std::wstring& GetClassName() const { return m_ClassName; }
	inline const std::wstring& GetWindowName() const { return m_WindowName; }
	inline unsigned int GetWindowWidth() const { return m_WindowWidth; }
	inline unsigned int GetWindowHeight() const { return m_WindowHeight; }
	inline Mouse& GetMouse() { return m_Mouse; }
	inline Keyboard& GetKeyboard() { return m_Keyboard; }

	void SetTitle(std::wstring Title);
	void AppendToTitle(std::wstring Message);
private:
	unsigned int m_WindowWidth;
	unsigned int m_WindowHeight;
	HWND m_WindowHandle;
	std::wstring m_ClassName;
	std::wstring m_WindowName;
	bool m_CursorEnabled;
	Mouse m_Mouse;
	Keyboard m_Keyboard;

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
	} m_ImGuiContextManager;
};