

#include "Application.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include <cstdint>
#include <filesystem>
#include <iostream>

#include "Renderer.hpp"

Application::Application(const std::filesystem::path& path) :
	m_renderer(nullptr) {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "Failed to initialize SDL: " << SDL_GetError()
				  << std::endl;
		throw;
	}

	m_window = SDL_CreateWindow(
		"SDLVulk Test", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
	);
	if (!m_window) {
		std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
		SDL_Quit();
		throw;
	}
	m_renderer = Renderer(m_window);
	m_renderer.load(path);
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

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_EVENT_QUIT) {
				running = false;
			}
			if (event.type == SDL_EVENT_KEY_DOWN) {
				auto keysympressed = event.key.key;
				if (event.key.key == SDLK_ESCAPE) {
					running = false;
				}
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
			    event.button.button == SDL_BUTTON_RIGHT) {
				SDL_GetMouseState(&prelockCoordinates.x, &prelockCoordinates.y);
				SDL_SetWindowRelativeMouseMode(m_window, true);
				SDL_WarpMouseInWindow(nullptr, 400, 300);
				cameraLocked = true;
			}
			if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
			    event.button.button == SDL_BUTTON_RIGHT) {
				SDL_SetWindowRelativeMouseMode(m_window, false);
				SDL_WarpMouseInWindow(
					nullptr, prelockCoordinates.x, prelockCoordinates.y
				);
				cameraLocked = false;
			}

			if (cameraLocked) {
				glm::vec2 coordinates;
				SDL_GetMouseState(&coordinates.x, &coordinates.y);
				m_renderer.getCamera().rotate(
					(glm::vec2(400., 300.) - coordinates) * deltaTime

				);
				SDL_WarpMouseInWindow(nullptr, 400, 300);
			}
		}

		m_renderer.render();
	};

	return 0;
}

Application::~Application() {
	SDL_DestroyWindow(m_window);
	SDL_Quit();
};