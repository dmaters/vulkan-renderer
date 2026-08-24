#include "UI.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <vulkan/vulkan.hpp>

#include "Instance.hpp"

using namespace UI;
UI::UIData UI::Data = {};
void UI::Setup() {
	vk::PhysicalDeviceProperties2 properties = Instance::Get().physicalDevice.getProperties2();

	Data.systemData.currentGpu = std::string(properties.properties.deviceName);
}

void UI::Render(vk::CommandBuffer commandBuffer) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Controls", &Data._utils.showControlsWindow);
	ImGui::Text("Right click for camera control");
	ImGui::Text("WASD for movement");
	ImGui::Text("LShift + WASD for faster movement");
	ImGui::Text("LCTRL + WASD for slower movement");
	ImGui::End();

	ImGui::SeparatorText("System Info");
	ImGui::LabelText("Current GPU", Data.systemData.currentGpu.c_str());

	ImGui::SeparatorText("Scene Data");
	ImGui::LabelText("Current scene", Data.sceneData.scenePath.c_str());
	ImGui::LabelText(
		"Primitives (visible/total)", "%i / %i", Data.sceneData.gbufferCount, Data.sceneData.primitiveCount
	);

	ImGui::SeparatorText("Lighting Data");
	ImGui::SliderAngle("Sun Angle", &Data.lightingData.sunAngleRad, -8, 188);

	ImGui::SeparatorText("Performance");
	ImGui::LabelText("Frame Rate", "%i fps", Data.performanceData.frameRate);

	ImGui::LabelText("CPU", "%.3f ms", Data.performanceData.cpuFrameTime);
	ImGui::LabelText("GPU", "%.3f ms", Data.performanceData.gpuFrameTime);

	if (ImGui::TreeNode("Timings")) {
		for (auto& [name, value] : Data.performanceData.taskFrameTime) {
			ImGui::LabelText(name.data(), "%.3f ms", value);
		}

		ImGui::TreePop();
	}

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
