#include "UI.hpp"

#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <vulkan/vulkan.hpp>
using namespace UI;

void UI::Render(vk::CommandBuffer commandBuffer, UIParameters& parameters) {
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplSDL3_NewFrame();
	ImGui::NewFrame();
	ImGui::SeparatorText("Scene Data");
	ImGui::LabelText("Current scene", parameters.sceneData.scenePath.c_str());
	ImGui::LabelText(
		"Primitives (visible/total)",
		"%i / %i",
		parameters.sceneData.gbufferCount,
		parameters.sceneData.primitiveCount
	);

	ImGui::SeparatorText("Lighting Data");
	ImGui::SliderAngle(
		"Sun Angle", &parameters.lightingData.sunAngleRad, -8, 188
	);

	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}
