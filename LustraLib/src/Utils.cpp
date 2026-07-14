#include "Utils.h"

#include "Logger.h"

#include <cstdlib> // wcstomb, mbstowcs
#include <cstring>

static const size_t sMaxSize = 4ull * Utils::MemoryUnitKB; // 4kB set as limit.
constexpr const char* sTooMuchMemoryErrorMessage =
    "String conversion required too much memory (over {} bytes). Returning empty string.";

namespace Utils
{
	std::string ConvertStringWideToNarrow(const wchar_t* wideString)
	{
		const size_t wideSize = wcslen(wideString) + 1; // Include null terminator;

		// Make enough space to hold characters spanning more than one byte.
		const size_t narrowSize = wideSize * sizeof(wchar_t);

		if (narrowSize > sMaxSize)
		{
			PRINT_ERROR(sTooMuchMemoryErrorMessage, sMaxSize);
			return "";
		}

		char narrowString[sMaxSize] = {};

		// Excluding null terminator.
		const size_t numberOfCharsWritten =
		    std::wcstombs(narrowString, wideString, narrowSize); // NOLINT(concurrency-mt-unsafe)

		if (numberOfCharsWritten == SIZE_MAX)
		{
			PRINT_ERROR("Could not convert wide string into narrow string. Returning empty string.");
			return "";
		}

		return std::string(narrowString, numberOfCharsWritten);
	}

	std::wstring ConvertStringNarrowToWide(const char* narrowString)
	{
		const size_t narrowLength = strlen(narrowString) + 1; // Include null terminator

		if (narrowLength * sizeof(wchar_t) > sMaxSize)
		{
			PRINT_ERROR(sTooMuchMemoryErrorMessage, sMaxSize);
			return L"";
		}

		const size_t wideStringMaxCharacters        = sMaxSize / sizeof(wchar_t);
		wchar_t wideString[wideStringMaxCharacters] = {};

		// Excluding null terminator.
		const size_t numberOfCharsWritten = std::mbstowcs(wideString, narrowString, wideStringMaxCharacters);
		if (numberOfCharsWritten == SIZE_MAX)
		{
			PRINT_ERROR("Could not convert narrow string into wide string. Returning empty string.");
			return L"";
		}

		return std::wstring(wideString, numberOfCharsWritten);
	}

} // namespace Utils
