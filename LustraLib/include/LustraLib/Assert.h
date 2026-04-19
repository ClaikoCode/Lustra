#pragma once

#include "Logger.h"

#include <cstdlib>

// Assert handling in Lustra is split up into 3 distinct categories:
// * Check
// * Validate
// * Ensure
//
// Check is the most basic of asserts. It is meant to be used in debug builds as a so-called "sanity check" for
// different scenarios. Failing an assert will halt the program and print an error. Checks will be seen as a no-op in
// release builds, which also means that any expression in Check asserts will NOT be evaluated. This behavior can be
// overwritten if the ENABLE_NON_DEBUG_CHECKS macro is defined.
//
// Validate is similar to Check, halting the program in debug builds, but will evaluate the expression that is being
// asserted in ALL build configurations. In non-debug builds, Validate will not halt the program but will still log the
// error. This behavior can be overwitten by defining the NON_LOGGING_VALIDATIONS macro, which still evaluates the
// expression but does not write any errors to the log.
//
// Ensure is the harshest assert which will always evaluate an expression and will cause a crash as well as log the
// error if the assertion failed in ALL build configurations. It is meant to be aplied to fatal errors that cannot be
// recovered from.
//
// All of these asserts try to insert a debug trap (a breakpoint) if the assertion failed as their default behavior.
// There is also a regular "raw" assert available which is essentially Ensure but without any logging.
//
// This assert usage pattern is inspired from how Unreal Engine 5.7 handles asserts. It is my own interpretation of
// using asserts in a way to shows intent and also allows for code to be better optimized in non-debug builds.

// Do while is to introduce a scope which has to be ended with a semicolon.
#define _NO_OP_MACRO()                                                                                                 \
	do                                                                                                                 \
	{                                                                                                                  \
	} while (false)

#define _THREAD_SAFE_EXIT() std::quick_exit(1)

// Only call debug trap if in a debug configuration.
#if defined(_DEBUG)
	#define _DEBUG_TRAP() __builtin_debugtrap()
#else
	#define _DEBUG_TRAP() _NO_OP_MACRO
#endif

// ----- CHECK assertion base macros -----
#if defined(_DEBUG) || defined(ENABLE_NON_DEBUG_CHECKS)

    // Evalute expression, log, halt program, and exit.
	#define _LUSTRA_CHECK_BASE(expression, message)                                                                    \
		do                                                                                                             \
		{                                                                                                              \
			if ((expression) == false)                                                                                 \
			{                                                                                                          \
				PRINT_ERROR("[CHECK ASSERTION FAILED] " message);                                                      \
				_DEBUG_TRAP();                                                                                         \
				_THREAD_SAFE_EXIT();                                                                                   \
			}                                                                                                          \
		} while (false)

#else

    // Do nothing.
	#define _LUSTRA_CHECK_BASE(expression, message) _NO_OP_MACRO

#endif
// -------------------------------------------

// ----- VALIDATE assertion base macros -----
#if defined(_DEBUG)

    // Validate expression, log, halt program, and exit.
	#define _LUSTRA_VALIDATE_BASE(expression, message)                                                                 \
		do                                                                                                             \
		{                                                                                                              \
			if ((expression) == false)                                                                                 \
			{                                                                                                          \
				PRINT_ERROR("[VALIDATE ASSERTION FAILED] " message);                                                   \
				_DEBUG_TRAP();                                                                                         \
				_THREAD_SAFE_EXIT();                                                                                   \
			}                                                                                                          \
		} while (false)

#elif defined(NON_LOGGING_VALIDATIONS)

    // Only evaluate expression.
	#define _LUSTRA_VALIDATE_BASE(expression, message) (expression)

#else

    // Validate expression, and log.
	#define _LUSTRA_VALIDATE_BASE(expression, message)                                                                 \
		do                                                                                                             \
		{                                                                                                              \
			if ((expression) == false)                                                                                 \
			{                                                                                                          \
				PRINT_ERROR("[VALIDATE ASSERTION FAILED] " message);                                                   \
			}                                                                                                          \
		} while (false)

#endif
// -------------------------------------------

// ----- ENSURE assertion base macros -----

// Validate expression, log, halt program, and exit.
#define _LUSTRA_ENSURE_BASE(expression, message)                                                                       \
	do                                                                                                                 \
	{                                                                                                                  \
		if ((expression) == false)                                                                                     \
		{                                                                                                              \
			PRINT_ERROR("[ENSURE ASSERTION FAILED] " message);                                                         \
			_DEBUG_TRAP();                                                                                             \
			_THREAD_SAFE_EXIT();                                                                                       \
		}                                                                                                              \
	} while (false)

// -------------------------------------------

// CHECK assert macros
#define CHECK_UNREACHABLE() _LUSTRA_CHECK_BASE(false, "Unreachable code was reached.")
#define CHECK_NOT_IMPL() _LUSTRA_CHECK_BASE(false, "Unimplemented code.")
#define CHECK_EX(expression, message) _LUSTRA_CHECK_BASE(expression, "(" #expression "): " message)
#define CHECK(expression) _LUSTRA_CHECK_BASE(expression, "(" #expression ")")

// VALIDATE assert macros
#define VALIDATE_EX(expression, message) _LUSTRA_VALIDATE_BASE(expression, "(" #expression "):" message)
#define VALIDATE(expression) _LUSTRA_VALIDATE_BASE(expression, "(" #expression ")")

// ENSURE assert macros
#define ENSURE_EX(expression, message) _LUSTRA_VALIDATE_BASE(expression, "(" #expression "):" message)
#define ENSURE(expression) _LUSTRA_VALIDATE_BASE(expression, "(" #expression ")")

// Pure assert
#define LUSTRA_ASSERT(expression)                                                                                      \
	do                                                                                                                 \
	{                                                                                                                  \
		if ((expression) == false)                                                                                     \
		{                                                                                                              \
			_DEBUG_TRAP();                                                                                             \
			_THREAD_SAFE_EXIT();                                                                                       \
		}                                                                                                              \
	} while (false)
