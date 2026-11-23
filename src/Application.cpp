#include "Application.hpp"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>

#include "Renderer.hpp"

static void check_vk_result(VkResult err) {
	if (err == 0) return;
	fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
	if (err < 0) abort();
}

Application::Application(const std::filesystem::path& path) {
	Instance::Create(m_window.getWindow());

	m_renderer = std::make_unique<Renderer>();
	m_renderer->load(path);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplGlfw_InitForVulkan(&m_window.getWindow(), false);
	Instance& instance = Instance::Get();
	ImGui_ImplVulkan_LoadFunctions(
		vk::ApiVersion13,
		[](const char* function_name, void* vulkan_instance) {
			return static_cast<vk::Instance*>(vulkan_instance)->getProcAddr(function_name);
		},
		static_cast<void*>(&instance.instance)
	);
	vk::DescriptorPoolSize poolSize = {
		.type = vk::DescriptorType::eCombinedImageSampler,
		.descriptorCount = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
	};
	auto [status, descriptorPool] = instance.device.createDescriptorPool(
		vk::DescriptorPoolCreateInfo {
			.maxSets = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
			.poolSizeCount = 1,
			.pPoolSizes = &poolSize,
		}
	);
	ImGui_ImplVulkanH_Window imguiWindow;
	imguiWindow.Surface = instance.surface;
	imguiWindow.Swapchain = instance.swapchain.getSwapchain();

	VkFormat outputFormat = VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	ImGui_ImplVulkan_InitInfo init_info = {
		.Instance = instance.instance,
		.PhysicalDevice = instance.physicalDevice,
		.Device = instance.device,
		.QueueFamily = instance.queueFamiliesIndices.graphicsIndex,
		.Queue = instance.graphicQueue,
		.DescriptorPool = descriptorPool,
		.MinImageCount = 3,
		.ImageCount = 3,
		.PipelineInfoMain = {
		    .PipelineRenderingCreateInfo = {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
				.colorAttachmentCount = 1,
				.pColorAttachmentFormats = &outputFormat,
      		},
      	 },
		.UseDynamicRendering = true,
        .CheckVkResultFn = check_vk_result,
	};
	ImGui_ImplVulkan_Init(&init_info);
}

int Application::run() {
	auto lastFrameTime = std::chrono::steady_clock::now();

	bool running = true;
	glm::vec2 mousePos;

	while (running) {
		m_window.update();
		auto inputState = m_window.getState();

		running = inputState.running;

		auto currentTime = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();

		lastFrameTime = currentTime;
		Camera& camera = m_renderer->getScene().camera;

		glm::vec3 direction = glm::vec3(0);
		float accel = 1.0;
		if (inputState.forward) direction += glm::vec3(0, 0, -1);
		if (inputState.backward) direction += glm::vec3(0, 0, 1);
		if (inputState.left) direction += glm::vec3(-1, 0, 0);
		if (inputState.right) direction += glm::vec3(1, 0, 0);
		if (inputState.speedBoost) accel = 10.0;
		if (inputState.speedSlow) accel = 0.2;

		glm::vec3 dir2 = direction * direction;
		if ((dir2.x + dir2.y + dir2.z) > 0)
			camera.position += camera.getOrientation() * (glm::normalize(direction) * deltaTime * accel * 150.0f);

		if (glm::ivec2(inputState.width, inputState.height) != m_renderer->getResolution())
			m_renderer->setResolution(inputState.width, inputState.height);

		if (inputState.cameraControl) {
			auto deltaPos = (inputState.mousePos - mousePos) * deltaTime;

			camera.yaw += -deltaPos.x * 10;
			camera.pitch += -deltaPos.y * 10;
			camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);
		}
		mousePos = inputState.mousePos;

		m_renderer->render();
	};

	return 0;
}

