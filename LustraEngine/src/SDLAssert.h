#include "LustraLib/Assert.h"
#include "LustraLib/Logger.h"
#include "SDL3/SDL.h"

// Asserts an SDL result and reports a custom message as well as the SDL error before halting the program.
#define ASSERT_SDL(SDL_expression, message)                                                                            \
	do                                                                                                                 \
		if ((SDL_expression) == false)                                                                                 \
		{                                                                                                              \
			PRINT_ERROR(message ": {}.", SDL_GetError());                                                              \
			LUSTRA_ASSERT((SDL_expression));                                                                           \
		}                                                                                                              \
	while (false)
