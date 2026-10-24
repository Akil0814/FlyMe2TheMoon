#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/game_module.h"

#include <cstdlib>

int main(int argc, char** argv)
{
	try
	{
		game::application::GameModule game_module;
		if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
			return EXIT_FAILURE;
		return ELYSIA_RUN_APP == elysia::application::ApplicationRunResult::NormalExit
			? EXIT_SUCCESS: EXIT_FAILURE;
	}
	catch (...)
	{
		return EXIT_FAILURE;
	}
}
