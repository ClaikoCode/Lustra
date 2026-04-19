#include "Utils.h"

#include "Logger.h"

#include <cstdlib> // wcstomb_s, mbstowcs_s
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

		char narrowString[sMaxSize]   = {};
		size_t convertedNumberOfChars = 0; // Does not include null terminator.
		if (wcstombs_s(&convertedNumberOfChars, narrowString, narrowSize, wideString, _TRUNCATE) != 0)
		{
			PRINT_ERROR("Could not convert wide string into narrow string. Returning empty string.");
			return "";
		}

		// Ignore null terminator as the string constructor will copy it as if it is a unique character part of the
		// string itself.
		return std::string(narrowString, convertedNumberOfChars - 1);
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
		size_t convertedNumberOfChars               = 0;
		if (mbstowcs_s(&convertedNumberOfChars, wideString, wideStringMaxCharacters, narrowString, _TRUNCATE) != 0)
		{
			PRINT_ERROR("Could not convert narrow string into wide string. Returning empty string.");
			return L"";
		}

		// Ignore null terminator as the string constructor will copy it as if it is a unique character part of the
		// string itself.
		return std::wstring(wideString, convertedNumberOfChars - 1);
	}
} // namespace Utils
