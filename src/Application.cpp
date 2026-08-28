

#include "Application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <backends/imgui_impl_sdl3.h>
#include <backends/imgui_impl_sdlgpu3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <cstdint>
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
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
		throw;
	}

	m_window = SDL_CreateWindow("SDLVulk Test", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!m_window) {
		std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw;
	}
	m_renderer = std::make_unique<Renderer>(m_window);
	m_renderer->load(path);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplSDL3_InitForVulkan(m_window);

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
	vk::DescriptorPool imguiPool = instance.device.createDescriptorPool(
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
		.DescriptorPool = imguiPool,
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
	SDL_Event event;
	bool running = true;
	bool cameraLocked = false;
	glm::vec2 prelockCoordinates;

	SDL_SetHint(SDL_HINT_MOUSE_RELATIVE_WARP_MOTION, "1");

	uint64_t lastFrameTime = 0;

	while (running) {
		uint64_t currentTime = SDL_GetTicks();
		float deltaTime = static_cast<float>(currentTime - lastFrameTime) / 1e3;
		lastFrameTime = currentTime;
		Camera& camera = m_renderer->getScene().camera;
		glm::vec2 centerCoords = glm::vec2(m_renderer->getResolution()) / glm::vec2(2.0);

		while (SDL_PollEvent(&event)) {
			ImGui_ImplSDL3_ProcessEvent(&event);

			if (event.type == SDL_EVENT_WINDOW_RESIZED) {
				m_renderer->setResolution(event.window.data1, event.window.data2);
			}

			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}

			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_RIGHT) {
				SDL_GetMouseState(&prelockCoordinates.x, &prelockCoordinates.y);
				SDL_SetWindowRelativeMouseMode(m_window, true);
				SDL_HideCursor();
				SDL_WarpMouseInWindow(nullptr, centerCoords.x, centerCoords.y);
				cameraLocked = true;
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_RIGHT) {
				SDL_SetWindowRelativeMouseMode(m_window, false);
				SDL_ShowCursor();
				SDL_WarpMouseInWindow(nullptr, prelockCoordinates.x, prelockCoordinates.y);
				cameraLocked = false;
			}
		}

		const bool* keyStates = SDL_GetKeyboardState(NULL);
		glm::vec3 direction = glm::vec3(0);
		float accel = 1.0;
		if (keyStates[SDL_SCANCODE_W]) direction += glm::vec3(0, 0, -1);
		if (keyStates[SDL_SCANCODE_S]) direction += glm::vec3(0, 0, 1);
		if (keyStates[SDL_SCANCODE_D]) direction += glm::vec3(1, 0, 0);
		if (keyStates[SDL_SCANCODE_A]) direction += glm::vec3(-1, 0, 0);
		if (keyStates[SDL_SCANCODE_LSHIFT]) accel = 10.0;
		if (keyStates[SDL_SCANCODE_LCTRL]) accel = 0.2;

		glm::vec3 dir2 = direction * direction;
		if ((dir2.x + dir2.y + dir2.z) > 0)
			camera.position += camera.getOrientation() * (glm::normalize(direction) * deltaTime * accel * 150.0f);

		if (cameraLocked) {
			glm::vec2 coordinates;
			SDL_GetMouseState(&coordinates.x, &coordinates.y);

			glm::vec2 rotation = (centerCoords - coordinates) * deltaTime;
			camera.yaw += rotation.x * 10;
			camera.pitch += rotation.y * 10;
			camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);
			SDL_WarpMouseInWindow(nullptr, centerCoords.x, centerCoords.y);
		}

		m_renderer->render();
	};

	return 0;
}

Application::~Application() {
	SDL_DestroyWindow(m_window);
	SDL_Quit();
};
