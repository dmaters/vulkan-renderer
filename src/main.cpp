
#include <SDL3/SDL_main.h>

#include "Application.hpp"

int main(int argc, char *argv[]) {
	assert(argc == 2);
	Application app((std::filesystem::path(std::string(argv[1]))));
	app.run();

	return 0;
}
