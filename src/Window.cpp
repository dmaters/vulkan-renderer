#include "Window.hpp"

#include <GLFW/glfw3.h>
#include <imgui_impl_glfw.h>

#include <iostream>
#include <vulkan/vulkan.hpp>

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);

	Window::InputState* inputState = (Window::InputState*)glfwGetWindowUserPointer(window);

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

	Window::InputState* inputState = (Window::InputState*)glfwGetWindowUserPointer(window);
	if (button != GLFW_MOUSE_BUTTON_RIGHT) return;

	if (action == GLFW_PRESS) {
		if (ImGui::GetIO().WantCaptureMouse) return;

		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		inputState->cameraControl = true;

	} else if (action == GLFW_RELEASE) {
		glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		inputState->cameraControl = false;
	}
}

Window::Window() {
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

	glfwSetKeyCallback(m_window, keyCallback);
	glfwSetMouseButtonCallback(m_window, mouseInputCallback);
	glfwSetWindowUserPointer(m_window, &m_inputState);
}

void Window::update() {
	glfwPollEvents();
	m_inputState.running &= !glfwWindowShouldClose(m_window);

	int width, height;
	glfwGetFramebufferSize(m_window, &width, &height);
	m_inputState.width = width;
	m_inputState.height = height;

	double x, y;
	glfwGetCursorPos(m_window, &x, &y);
	m_inputState.mousePos = glm::vec2(x, y);
}
