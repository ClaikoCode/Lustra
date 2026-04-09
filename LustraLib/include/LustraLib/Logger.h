#pragma once

#include <format>
#include <string>
#include <string_view>

// clang-format off
#define ANSI_BLACK		"\033[0;30m"
#define ANSI_RED		"\033[0;31m"
#define ANSI_GREEN		"\033[0;32m"
#define ANSI_YELLOW		"\033[0;33m"
#define ANSI_BLUE		"\033[0;34m"
#define ANSI_PURPLE		"\033[0;35m"
#define ANSI_CYAN		"\033[0;36m"
#define ANSI_WHITE		"\033[0;37m" // Same color as default text output.
#define ANSI_RESET		"\033[0m"
// clang-format on

#define COLOR_TEXT(color, text) (color text ANSI_RESET)

namespace LustraLib
{
	enum OutputLevel : uint32_t
	{
		OutputLevelTrace = 0u,
		OutputLevelDebug,
		OutputLevelLog,
		OutputLevelWarning,
		OutputLevelError,

		OutputLevelCount // Keep last!
	};

	// Global shared struct to hold logger options.
	inline struct LoggerOptions
	{
		OutputLevel currentOutputLevel = OutputLevelTrace;

		// Controlls if the full system path for the file should be printed.
		bool printFullPath = false;

		// Controlls if message should include source location (filepath, funcname, line).
		// Disabling this can be useful if the print location itself is not important/would be missleading.
		bool printSourceLocationInfo = true;

	} gLoggerOptions;

	// Takes an already formatted message and prints it together with an output header.
	// This function is thread-safe: a mutex is locked when printing to avoid concurrent writing to the same file.
	void PrintImpl(
	    OutputLevel outputLevel,
	    std::string_view filePath,
	    std::string_view funcName,
	    uint32_t line,
	    std::string_view formattedMessage
	);

	template <typename... Args>
	void Print(
	    OutputLevel outputLevel,
	    std::string_view filePath,
	    std::string_view funcName,
	    uint32_t line,
	    std::format_string<Args...> messageFormat,
	    Args&&... args
	)
	{
		std::string formattedMessage = std::format(messageFormat, std::forward<Args>(args)...);
		PrintImpl(outputLevel, filePath, funcName, line, formattedMessage);
	};

	void SetOutputLevel(OutputLevel outputLevel);
	OutputLevel GetOutputLevel();
} // namespace LustraLib

// clang-format off
#define PRINT_TRACE(message, ...)                                                                                      \
	LustraLib::Print(LustraLib::OutputLevelTrace, __FILE__, __func__, __LINE__, message __VA_OPT__(,) __VA_ARGS__)
#define PRINT_LOG(message, ...)                                                                                        \
	LustraLib::Print(LustraLib::OutputLevelLog, __FILE__, __func__, __LINE__, message __VA_OPT__(,) __VA_ARGS__)
#define PRINT_DEBUG(message, ...)                                                                                      \
	LustraLib::Print(LustraLib::OutputLevelDebug, __FILE__, __func__, __LINE__, message __VA_OPT__(,) __VA_ARGS__)
#define PRINT_WARNING(message, ...)                                                                                    \
	LustraLib::Print(LustraLib::OutputLevelWarning, __FILE__, __func__, __LINE__, message __VA_OPT__(,) __VA_ARGS__)
#define PRINT_ERROR(message, ...)                                                                                      \
	LustraLib::Print(LustraLib::OutputLevelError, __FILE__, __func__, __LINE__, message __VA_OPT__(,) __VA_ARGS__)
// clang-format on
