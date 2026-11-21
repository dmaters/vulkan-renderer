#pragma once

#include "Renderer.hpp"

struct GLFWwindow;
class Application {
private:
	std::unique_ptr<Renderer> m_renderer;
	GLFWwindow* m_window;

public:
	Application(const std::filesystem::path& path);
	int run();

	~Application();
};
