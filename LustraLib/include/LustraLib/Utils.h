#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

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

	enum class FileReadCode : uint8_t
	{
		FailedOpen,
		FailedRead
	};
	std::string ConvertStringWideToNarrow(const wchar_t* wideString);

	std::wstring ConvertStringNarrowToWide(const char* narrowString);

	std::expected<std::vector<std::byte>, FileReadCode> ReadFileBytes(const std::filesystem::path& filePath);

	// This classic hash combine is based on the boost library.
	inline void HashCombine(std::size_t& seed, std::size_t value)
	{
		seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
	}

	template <typename T>
	// Gets the typename as a string at compile time.
	// NOTE: In MSVC, the string that is parsed includes this function's name. If the func name changes, the string also
	// has to.
	constexpr std::string_view TypeName()
	{
#if defined(__clang__) || defined(__GNUC__)
		// Clang: "std::string_view TypeName() [T = Resource::Shader]"
		// GCC:   "... TypeName() [with T = Resource::Shader; std::string_view = ...]"

		constexpr std::string_view sig    = __PRETTY_FUNCTION__;
		constexpr std::string_view prefix = "T = ";
		const size_t start                = sig.find(prefix) + prefix.size();
		const size_t end                  = sig.find_first_of(";]", start); // GCC ends at ';', Clang at ']'

		return sig.substr(start, end - start);

#elif defined(_MSC_VER)
		// MSVC: "... TypeName<struct Resource::Shader>(void)"

		constexpr std::string_view sig    = __FUNCSIG__;
		constexpr std::string_view prefix = "TypeName<";
		const size_t start                = sig.find(prefix) + prefix.size();
		const size_t end                  = sig.rfind('>'); // last '>' closes the template arg
		std::string_view name             = sig.substr(start, end - start);

		// MSVC prefixes the elaborated keyword. Its stripped out here so it matches Clang/GCC output.
		for (std::string_view kw : {"struct ", "class ", "enum ", "union "})
			if (name.substr(0, kw.size()) == kw)
			{
				name.remove_prefix(kw.size());
				break;
			}

		return name;
#else
		return "unknown";
#endif
	}
} // namespace Utils
