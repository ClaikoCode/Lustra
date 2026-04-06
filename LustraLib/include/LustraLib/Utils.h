#pragma once

#include <string>

#define UNUSED_VAR(var) (void)var

namespace Utils
{
	// Divide number of bytes by the memory unit.
	// Useful when formatting output
	enum MemoryUnit : unsigned int
	{
		MemoryUnitKB = 1024u,
		MemoryUnitMB = MemoryUnitKB * 1024u,
		MemoryUnitGB = MemoryUnitMB * 1024u
	};

	std::string ConvertStringWideToNarrow(const wchar_t* wideString);

	std::wstring ConvertStringNarrowToWide(const char* narrowString);
} // namespace Utils
