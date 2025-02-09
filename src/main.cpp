
#include <SDL3/SDL_main.h>

#include "Application.hpp"

int main(int argv, char **args) {
	Application app("sample_meshes/cube.obj");
	app.run();

	return 0;
}
