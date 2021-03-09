#include "pch.h"
#include "RenderSystemWindow.h"

#include <Core/RenderSystem.h>
#include <Graphics/RenderDevice.h>
#include <Graphics/PathIntegrator.h>

void RenderSystemWindow::RenderGui()
{
	ImGuiIO& IO = ImGui::GetIO();

	ImGui::BeginChild("Render System##Editor", ImVec2(IO.DisplaySize.x * 0.25f, 0), true);
	ImGui::Text("Render System");
	ImGui::Separator();
	ImGui::Spacing();

	UIWindow::Update();

	const auto& AdapterDesc = RenderDevice::Instance().GetAdapterDesc();
	auto LocalVideoMemoryInfo = RenderDevice::Instance().QueryLocalVideoMemoryInfo();
	auto UsageInMiB = ToMiB(LocalVideoMemoryInfo.CurrentUsage);
	ImGui::Text("GPU: %ls", AdapterDesc.Description);
	ImGui::Text("VRAM Usage: %d Mib", UsageInMiB);

	ImGui::Text("");
	ImGui::Text("Total Frame Count: %d", RenderSystem::Statistics::TotalFrameCount);
	ImGui::Text("FPS: %f", RenderSystem::Statistics::FPS);
	ImGui::Text("FPMS: %f", RenderSystem::Statistics::FPMS);

	ImGui::Text("");

	if (ImGui::Button("Request Capture"))
	{
		pRenderSystem->OnRequestCapture();
	}

	if (ImGui::TreeNode("Settings"))
	{
		if (ImGui::Button("Restore Defaults"))
		{
			RenderSystem::Settings::RestoreDefaults();
		}
		ImGui::Checkbox("V-Sync", &RenderSystem::Settings::VSync);

		PathIntegrator::Settings::RenderGui();

		ImGui::TreePop();
	}
	ImGui::EndChild();
}