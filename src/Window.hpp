#pragma once
#include <GLFW/glfw3.h>

#include <cstddef>
#include <glm/glm.hpp>

class Window {
public:
	struct InputState {
		bool cameraControl;
		bool forward;
		bool backward;
		bool right;
		bool left;

		bool speedBoost;
		bool speedSlow;

		glm::vec2 mousePos;

		std::size_t width;
		std::size_t height;

		bool running = true;
	};

private:
	GLFWwindow* m_window;
	InputState m_inputState {};

public:
	Window();

	void update();

	GLFWwindow& getWindow() const { return *m_window; };
	const InputState& getState() const { return m_inputState; };
};
