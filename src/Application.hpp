#pragma once

#include "Renderer.hpp"
#include "Window.hpp"

class Application {
private:
	std::unique_ptr<Renderer> m_renderer;
	Window m_window;

public:
	Application(const std::filesystem::path& path);
	int run();

};
