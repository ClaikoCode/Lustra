#pragma once

#include <filesystem>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

class DirectoryWatcher
{
  public:
	DirectoryWatcher(
	    const std::filesystem::path& watchDirectory,
	    uint32_t pollInterval,
	    const std::vector<std::string_view>& extensionFilters = {}
	);
	~DirectoryWatcher();
	DirectoryWatcher()                                   = delete;
	DirectoryWatcher(const DirectoryWatcher&)            = delete;
	DirectoryWatcher(DirectoryWatcher&&)                 = delete;
	DirectoryWatcher& operator=(const DirectoryWatcher&) = delete;
	DirectoryWatcher& operator=(DirectoryWatcher&&)      = delete;

	void GetChangedFiles(std::unordered_set<std::filesystem::path>& changedFiles);

  private:
	// This function will be executed by a separate thread.
	void WatchDirectory();

	bool IsValidEntry(const std::filesystem::directory_entry& dirEntry);

	std::filesystem::path m_watchDirectory;
	uint32_t m_pollIntervalMS = 200u;

	std::vector<std::string_view> m_extensionFilters;

	std::unordered_set<std::filesystem::path> m_changedFiles;
	std::mutex m_changedFilesMutex;
	std::thread m_watcherThread;
	bool m_shouldQuitThread = false;
};
