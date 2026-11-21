#include "Application.hpp"

#include <GLFW/glfw3.h>
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
	if (!glfwInit()) {
		const char* error;
		glfwGetError(&error);
		std::cerr << "Couldn't initialize glfw. " << error << std::endl;
		abort();
	}
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = glfwCreateWindow(1280, 720, "Renderer", nullptr, nullptr);

	if (!m_window) {
		const char* error;
		glfwGetError(&error);
		std::cerr << "Couldn't create glfw window. " << error << std::endl;
		abort();
	}

	Instance::Create(m_window);

	m_renderer = std::make_unique<Renderer>();
	m_renderer->load(path);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	ImGui_ImplGlfw_InitForVulkan(m_window, true);
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

struct InputState {
	bool cameraControl;
	bool forward;
	bool backward;
	bool right;
	bool left;

	bool speedBoost;
	bool speedSlow;

	double mX;
	double mY;
};

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	InputState* inputState = (InputState*)glfwGetWindowUserPointer(window);

	if (action == GLFW_PRESS) {
		switch (key) {
			case GLFW_KEY_ESCAPE:
				glfwSetWindowShouldClose(window, GLFW_TRUE);
				break;
			case GLFW_KEY_W:
				inputState->forward = true;
				break;
			case GLFW_KEY_S:
				inputState->backward = true;
				break;
			case GLFW_KEY_A:
				inputState->left = true;
				break;
			case GLFW_KEY_D:
				inputState->right = true;
				break;
			case GLFW_KEY_LEFT_SHIFT:
				inputState->speedBoost = true;
				break;
			case GLFW_KEY_LEFT_CONTROL:
				inputState->speedSlow = true;
				break;
		}
	}

	else if (action == GLFW_RELEASE) {
		switch (key) {
			case GLFW_KEY_W:
				inputState->forward = false;
				break;
			case GLFW_KEY_S:
				inputState->backward = false;
				break;
			case GLFW_KEY_A:
				inputState->left = false;
				break;
			case GLFW_KEY_D:
				inputState->right = false;
				break;
			case GLFW_KEY_LEFT_SHIFT:
				inputState->speedBoost = false;
				break;
			case GLFW_KEY_LEFT_CONTROL:
				inputState->speedSlow = false;
				break;
		}
	}
}

static void mouseInputCallback(GLFWwindow* window, int button, int action, int mods) {
	ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);

	if (ImGui::GetIO().WantCaptureMouse) return;

	InputState* inputState = (InputState*)glfwGetWindowUserPointer(window);
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			inputState->cameraControl = true;
			glfwGetCursorPos(window, &inputState->mX, &inputState->mY);

		} else if (action == GLFW_RELEASE) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			inputState->cameraControl = false;
		}
	}
}

int Application::run() {
	auto lastFrameTime = std::chrono::steady_clock::now();

	InputState inputState {};
	glfwSetWindowUserPointer(m_window, &inputState);
	glfwSetKeyCallback(m_window, keyCallback);
	glfwSetMouseButtonCallback(m_window, mouseInputCallback);

	while (!glfwWindowShouldClose(m_window)) {
		glfwPollEvents();

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

		glm::ivec2 res;
		glfwGetFramebufferSize(m_window, &res.x, &res.y);
		if (res != m_renderer->getResolution()) m_renderer->setResolution(res.x, res.y);

		if (inputState.cameraControl) {
			double newX, newY;
			glfwGetCursorPos(m_window, &newX, &newY);
			float x = (newX - inputState.mX) * deltaTime;
			float y = (newY - inputState.mY) * deltaTime;

			inputState.mX = newX;
			inputState.mY = newY;

			camera.yaw += -x * 10;
			camera.pitch += -y * 10;
			camera.pitch = glm::clamp(camera.pitch, -89.0f, 89.0f);
		}

		m_renderer->render();
	};

	return 0;
}

Application::~Application() {
	glfwDestroyWindow(m_window);
	glfwTerminate();
};
