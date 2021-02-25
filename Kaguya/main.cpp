// main.cpp : Defines the entry point for the application.
//
#include "pch.h"

#if defined(_DEBUG)
// memory leak
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#define ENABLE_LEAK_DETECTION() _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
#define SET_LEAK_BREAKPOINT(x) _CrtSetBreakAlloc(x)
#else
#define ENABLE_LEAK_DETECTION() 0
#define SET_LEAK_BREAKPOINT(X) X
#endif

#define NOMINMAX
#include <Core/Application.h>
#include <Graphics/RenderDevice.h>
#include <Graphics/AssetManager.h>
#include <Graphics/Renderer.h>
#include <Graphics/UI/HierarchyWindow.h>
#include <Graphics/UI/ViewportWindow.h>
#include <Graphics/UI/InspectorWindow.h>
#include <Graphics/UI/RenderSystemWindow.h>
#include <Graphics/UI/AssetWindow.h>

#define SHOW_IMGUI_DEMO_WINDOW 1

class Editor
{
public:
	void Initialize()
	{
		HierarchyWindow.SetContext(&Scene);
		InspectorWindow.SetContext(&Scene, {});

		RenderSystemWindow.SetContext(&Renderer);

		Renderer.OnInitialize();
	}

	void Shutdown()
	{
		Renderer.OnDestroy();
	}

	void Render()
	{
		const Time& Time = Application::Time;
		Mouse& Mouse = Application::InputHandler.Mouse;
		Keyboard& Keyboard = Application::InputHandler.Keyboard;
		float DeltaTime = Time.DeltaTime();

		ImGuiIO& IO = ImGui::GetIO();

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
#if SHOW_IMGUI_DEMO_WINDOW
		ImGui::ShowDemoWindow();
#endif

		ImGui::SetNextWindowPos(ImVec2(0, 0));
		ImGui::SetNextWindowSize(IO.DisplaySize);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::Begin("Kaguya", nullptr, ImGuiWindowFlags_MenuBar
			| ImGuiWindowFlags_NoTitleBar
			| ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove
			| ImGuiWindowFlags_NoScrollbar
			| ImGuiWindowFlags_NoScrollWithMouse
			| ImGuiWindowFlags_NoCollapse
			| ImGuiWindowFlags_AlwaysAutoResize
			| ImGuiWindowFlags_NoBringToFrontOnFocus);
		{
			ImGui::Spacing();

			ImGui::BeginGroup();

			HierarchyWindow.RenderGui();
			ImGui::SameLine();

			ViewportWindow.RenderGui((void*)Renderer.GetViewportDescriptor().GpuHandle.ptr);

			RenderSystemWindow.RenderGui();
			ImGui::SameLine();

			AssetWindow.RenderGui();
			ImGui::SameLine();

			ImGui::EndGroup();
			ImGui::SameLine();

			InspectorWindow.SetContext(&Scene, HierarchyWindow.GetSelectedEntity());
			InspectorWindow.RenderGui(ViewportWindow.Rect.left, ViewportWindow.Rect.top, ViewportWindow.Resolution.x, ViewportWindow.Resolution.y);
		}

		ImGui::End();
		ImGui::PopStyleVar();

		auto [mX, mY] = ViewportWindow.GetMousePosition();
		uint32_t viewportWidth = ViewportWindow.Resolution.x, viewportHeight = ViewportWindow.Resolution.y;

		// Uncomment this and comment the viewport above for 1920x1080 captures
		//uint32_t viewportWidth = 1920, viewportHeight = 1080;

		// Update
		Scene.PreviousCamera = Scene.Camera;

		Scene.Camera.AspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

		// Update selected entity
		// If LMB is pressed and we are not handling raw input and if we are not hovering over any imgui stuff then we update the
		// instance id for editor
		if (Mouse.IsLMBPressed() && !Mouse.UseRawInput && ViewportWindow.IsHovered && !ImGuizmo::IsUsing())
		{
			HierarchyWindow.SetSelectedEntity(Renderer.GetSelectedEntity());
		}

		if (Mouse.UseRawInput)
		{
			if (Keyboard.IsKeyPressed('W'))
				Scene.Camera.Translate(0.0f, 0.0f, DeltaTime);
			if (Keyboard.IsKeyPressed('A'))
				Scene.Camera.Translate(-DeltaTime, 0.0f, 0.0f);
			if (Keyboard.IsKeyPressed('S'))
				Scene.Camera.Translate(0.0f, 0.0f, -DeltaTime);
			if (Keyboard.IsKeyPressed('D'))
				Scene.Camera.Translate(DeltaTime, 0.0f, 0.0f);
			if (Keyboard.IsKeyPressed('E'))
				Scene.Camera.Translate(0.0f, DeltaTime, 0.0f);
			if (Keyboard.IsKeyPressed('Q'))
				Scene.Camera.Translate(0.0f, -DeltaTime, 0.0f);

			while (const auto RawInput = Mouse.ReadRawInput())
			{
				Scene.Camera.Rotate(RawInput->Y * DeltaTime, RawInput->X * DeltaTime);
			}
		}

		Scene.Update();

		// Render
		Renderer.SetViewportMousePosition(mX, mY);
		Renderer.SetViewportResolution(viewportWidth, viewportHeight);
		Renderer.OnUpdate(Time);

		Renderer.OnRender(Scene);
	}

	void Resize(uint32_t Width, uint32_t Height)
	{
		Renderer.OnResize(Width, Height);
	}
private:
	HierarchyWindow		HierarchyWindow;
	ViewportWindow		ViewportWindow;
	InspectorWindow		InspectorWindow;
	RenderSystemWindow	RenderSystemWindow;
	AssetWindow			AssetWindow;

	Scene				Scene;
	Renderer			Renderer;
};

int main(int argc, char* argv[])
{
#if defined(_DEBUG)
	ENABLE_LEAK_DETECTION();
	SET_LEAK_BREAKPOINT(-1);
#endif

	Application::Config Config =
	{
		.Title = L"Kaguya",
		.Width = 1280,
		.Height = 720,
		.Maximize = true
	};

	Application::Initialize(Config);
	RenderDevice::Initialize();
	AssetManager::Initialize();

	RenderDevice::Instance().ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");

	auto pEditor = std::make_unique<Editor>();
	pEditor->Initialize();

	Application::Window.SetRenderFunc([&]()
	{
		Application::Time.Signal();
		pEditor->Render();
	});

	Application::Window.SetResizeFunc([&](UINT Width, UINT Height)
	{
		pEditor->Resize(Width, Height);
	});

	Application::Time.Restart();
	return Application::Run([&]()
	{
		pEditor->Shutdown();
		pEditor.reset();

		AssetManager::Shutdown();
		RenderDevice::Shutdown();
	});
}