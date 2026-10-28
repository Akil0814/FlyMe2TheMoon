#include <SDL3/SDL_main.h>
#include "engine/application/application.h"
#include "game/application/game_module.h"

#include <cstdlib>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

namespace {
void web_frame(void* context)
{
    auto* app = static_cast<elysia::application::Application*>(context);
    try {
        if (auto result = app->run_frame()) {
            emscripten_cancel_main_loop();
            const auto final_result = app->finish_run(*result);
            EM_ASM({ Module['onGameExit']?.($0); },
                final_result == elysia::application::ApplicationRunResult::NormalExit ? 0 : 1);
        }
    } catch (...) {
        emscripten_cancel_main_loop();
        app->finish_run(elysia::application::ApplicationRunResult::FaultExit);
        EM_ASM({ Module['onGameExit']?.(1); });
    }
}
}
#endif

int main(int argc, char** argv)
{
	try
	{
		static game::application::GameModule game_module;
#ifdef __EMSCRIPTEN__
        SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif
		if (!ELYSIA_INITIALIZE_APP(argc, argv, game_module))
        {
#ifdef __EMSCRIPTEN__
            EM_ASM({ Module['onGameExit']?.(1); });
#endif
			return EXIT_FAILURE;
        }
#ifdef __EMSCRIPTEN__
        ELYSIA_APP->begin_run();
        emscripten_set_main_loop_arg(web_frame, ELYSIA_APP, 0, false);
        return EXIT_SUCCESS;
#else
		return ELYSIA_RUN_APP == elysia::application::ApplicationRunResult::NormalExit
			? EXIT_SUCCESS: EXIT_FAILURE;
#endif
	}
	catch (...)
	{
#ifdef __EMSCRIPTEN__
        EM_ASM({ Module['onGameExit']?.(1); });
#endif
		return EXIT_FAILURE;
	}
}
