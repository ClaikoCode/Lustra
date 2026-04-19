#include "Logger.h"

#include <cstdio>  // printf, sprintf
#include <cstring> // std::memset
#include <mutex>
#include <print>

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
	    OutputLevel outputLevel,
	    std::string_view filePath,
	    std::string_view funcName,
	    uint32_t line,
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

		if (gLoggerOptions.printSourceLocationInfo)
		{
			// Default to regular filepath
			std::string_view fileName = filePath;

			// Avoids printing the whole path. Makes output tidier.
			if (!gLoggerOptions.printFullPath)
			{
				const size_t fileNameStartPos = filePath.find_last_of('/');
				if (fileNameStartPos != std::string_view::npos)
				{
					fileName = filePath.substr(fileNameStartPos + 1);
				}
			}

			{
				const std::scoped_lock printingLock(sPrintingMutex);

				std::print(
				    outputFile,
				    "[{}] ({} -> {}:{}): {}\n",
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
			{
				const std::scoped_lock printingLock(sPrintingMutex);

				std::print(outputFile, "[{}] {}\n", outputLevelString, formattedMessage.data());
			}
		}
	}
} // namespace LustraLib
