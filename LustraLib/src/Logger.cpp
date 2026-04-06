#include "Logger.h"

#include <cstdio>  // printf, sprintf
#include <cstring> // std::memset
#include <mutex>

namespace LustraLib
{
	// clang-format off
	constexpr const char* ceOutputLevelString[OutputLevelCount] = {
	    COLOR_TEXT(ANSI_WHITE,	"TRACE"),
	    COLOR_TEXT(ANSI_PURPLE, "DEBUG"),
	    COLOR_TEXT(ANSI_CYAN, 	"LOG"),
	    COLOR_TEXT(ANSI_YELLOW, "WARNING"),
	    COLOR_TEXT(ANSI_RED, 	"ERROR")
	};
	// clang-format on

	void PrintImpl(
	    OutputLevel outputLevel, std::string_view filePath, std::string_view funcName, uint32_t line,
	    std::string_view formattedMessage
	)
	{
		static std::mutex sPrintingMutex = {};

		if (outputLevel < gLoggerOptions.currentOutputLevel)
		{
			return; // Skip all logic if outputlevel is below the global level.
		}

		const char* outputLevelString =
		    outputLevel < OutputLevelCount ? ceOutputLevelString[outputLevel] : "UNKNOWN OUTPUT LEVEL";
		FILE* const outputFile = outputLevel == OutputLevelError ? stderr : stdout;

		int result = 0;
		if (gLoggerOptions.printSourceLocationInfo)
		{
			const char* headerFmt = "[%s] (%s -> %s:%u): %s\n";

			// Default to regular filepath
			std::string_view fileName = filePath;

			// Avoids printing the whole path. Makes output tidier.
			if (!gLoggerOptions.printFullPath)
			{
				size_t fileNameStartPos = filePath.find_last_of("/");
				if (fileNameStartPos != std::string_view::npos)
				{
					fileName = filePath.substr(fileNameStartPos + 1);
				}
			}

			{
				std::scoped_lock printingLock(sPrintingMutex);

				result = fprintf(
				    outputFile,
				    headerFmt,
				    outputLevelString,
				    funcName.data(),
				    fileName.data(),
				    line,
				    formattedMessage.data()
				);
			}
		}
		else
		{
			const char* headerFmt = "[%s] %s\n";

			{
				std::scoped_lock printingLock(sPrintingMutex);

				result = fprintf(outputFile, headerFmt, outputLevelString, formattedMessage.data());
			}
		}

		if (result < 0)
		{
			fprintf(stderr, "fprintf() failed to output logging string!");
		}
	}
} // namespace LustraLib
