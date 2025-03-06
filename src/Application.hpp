#pragma once

#include "Renderer.hpp"
#include "SDL3/SDL_video.h"

class Application {
private:
	SDL_Window* m_window;
	std::unique_ptr<Renderer> m_renderer;

public:
	Application(const std::filesystem::path& path);
	int run();

	~Application();
};