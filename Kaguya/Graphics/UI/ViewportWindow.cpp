#include "pch.h"
#include "ViewportWindow.h"

std::pair<float, float> ViewportWindow::GetMousePosition() const
{
	auto [mx, my] = ImGui::GetMousePos();
	mx -= Rect.left;
	my -= Rect.top;

	return { mx, my };
}

void ViewportWindow::RenderGui(void* pImage)
{
	ImGuiIO& IO = ImGui::GetIO();

	ImGui::BeginChild("Viewport##Editor", ImVec2(IO.DisplaySize.x * 0.6f, IO.DisplaySize.y * 0.7f), true, ImGuiWindowFlags_NoScrollbar);
	ImGui::Text("Viewport");
	ImGui::Separator();
	ImGui::Spacing();

	UIWindow::Update();

	auto viewportOffset = ImGui::GetCursorPos(); // includes tab bar
	auto viewportPos = ImGui::GetWindowPos();
	auto viewportSize = ImGui::GetContentRegionAvail();
	auto windowSize = ImGui::GetWindowSize();

	Rect.left = viewportPos.x + viewportOffset.x;
	Rect.top = viewportPos.y + viewportOffset.y;
	Rect.right = Rect.left + windowSize.x;
	Rect.bottom = Rect.top + windowSize.y;

	Resolution = Vector2i(viewportSize.x, viewportSize.y);

	ImGui::Image((ImTextureID)pImage, viewportSize);

	if (ImGui::IsMouseDown(ImGuiMouseButton_Right) && IsHovered)
	{
		Application::InputHandler.Mouse.UseRawInput = true;
	}
	else
	{
		Application::InputHandler.Mouse.UseRawInput = false;
	}
	ImGui::EndChild();
}