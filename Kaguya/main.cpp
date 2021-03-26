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
	Editor()
	{
		HierarchyWindow.SetContext(&Scene);
		InspectorWindow.SetContext(&Scene, {});

		RenderSystemWindow.SetContext(&Renderer);

		AssetWindow.pScene = &Scene;

		Renderer.OnInitialize();
	}

	~Editor()
	{
		Renderer.OnDestroy();
	}

	void Render()
	{
		const Time& time = Application::Time;
		Mouse& mouse = Application::InputHandler.Mouse;
		Keyboard& keyboard = Application::InputHandler.Keyboard;
		float dt = time.DeltaTime();

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
		ImGui::Begin("Kaguya", nullptr,
			ImGuiWindowFlags_MenuBar |
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_AlwaysAutoResize |
			ImGuiWindowFlags_NoBringToFrontOnFocus);
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

		auto [vpX, vpY] = ViewportWindow.GetMousePosition();
		uint32_t viewportWidth = ViewportWindow.Resolution.x, viewportHeight = ViewportWindow.Resolution.y;

		// Uncomment this and comment the viewport above for 1920x1080 captures
		//uint32_t viewportWidth = 1920, viewportHeight = 1080;

		// Update
		Scene.PreviousCamera = Scene.Camera;

		Scene.Camera.AspectRatio = static_cast<float>(viewportWidth) / static_cast<float>(viewportHeight);

		// Update selected entity
		// If LMB is pressed and we are not handling raw input and if we are not hovering over any imgui stuff then we update the
		// instance id for editor
		if (mouse.IsLMBPressed() && !mouse.UseRawInput && ViewportWindow.IsHovered && !ImGuizmo::IsUsing())
		{
			HierarchyWindow.SetSelectedEntity(Renderer.GetSelectedEntity());
		}

		if (mouse.UseRawInput)
		{
			if (keyboard.IsKeyPressed('W'))
				Scene.Camera.Translate(0.0f, 0.0f, dt);
			if (keyboard.IsKeyPressed('A'))
				Scene.Camera.Translate(-dt, 0.0f, 0.0f);
			if (keyboard.IsKeyPressed('S'))
				Scene.Camera.Translate(0.0f, 0.0f, -dt);
			if (keyboard.IsKeyPressed('D'))
				Scene.Camera.Translate(dt, 0.0f, 0.0f);
			if (keyboard.IsKeyPressed('E'))
				Scene.Camera.Translate(0.0f, dt, 0.0f);
			if (keyboard.IsKeyPressed('Q'))
				Scene.Camera.Translate(0.0f, -dt, 0.0f);

			while (const auto rawInput = mouse.ReadRawInput())
			{
				Scene.Camera.Rotate(rawInput->Y * dt, rawInput->X * dt);
			}
		}

		Scene.Update();

		// Render
		Renderer.SetViewportMousePosition(vpX, vpY);
		Renderer.SetViewportResolution(viewportWidth, viewportHeight);

		Renderer.OnRender(time, Scene);
	}

	void Resize(uint32_t Width, uint32_t Height)
	{
		Renderer.OnResize(Width, Height);
	}
private:
	Scene				Scene;
	Renderer			Renderer;

	HierarchyWindow		HierarchyWindow;
	ViewportWindow		ViewportWindow;
	InspectorWindow		InspectorWindow;
	RenderSystemWindow	RenderSystemWindow;
	AssetWindow			AssetWindow;
};

int main(int argc, char* argv[])
{
#if defined(_DEBUG)
	ENABLE_LEAK_DETECTION();
	SET_LEAK_BREAKPOINT(-1);
#endif

	Application::Config config =
	{
		.Title = L"Kaguya",
		.Width = 1280,
		.Height = 720,
		.Maximize = true
	};

	Application::Initialize(config);
	RenderDevice::Initialize();
	AssetManager::Initialize();

	RenderDevice::Instance().ShaderCompiler.SetIncludeDirectory(Application::ExecutableFolderPath / L"Shaders");

	auto editor = std::make_unique<Editor>();

	Application::Window.SetRenderFunc([&]()
	{
		Application::Time.Signal();
		editor->Render();
	});

	Application::Window.SetResizeFunc([&](UINT Width, UINT Height)
	{
		editor->Resize(Width, Height);
	});

	Application::Time.Restart();
	return Application::Run([&]()
	{
		editor.reset();

		AssetManager::Shutdown();
		RenderDevice::Shutdown();
	});
}