#include "DirectoryWatcher.h"

#include "Assert.h"
#include "Logger.h"

#include <algorithm>
#include <chrono>
#include <unordered_map>

namespace fs = std::filesystem;

void DirectoryWatcher::WatchDirectory()
{
	if (!fs::is_directory(m_watchDirectory))
	{
		PRINT_ERROR("'{}' is not a directory. Aborting directory watching.", m_watchDirectory.string());
		return;
	}

	PRINT_DEBUG("Watching directory '{}'.", m_watchDirectory.string());

	if (m_pollIntervalMS < 100)
	{
		PRINT_WARNING(
		    "Polling directory at over 10Hz, potentially causing thread to consume unecessary cycles. Consider if such "
		    "high frequency is needed and if event driven choice would fit better."
		);
	}

	std::unordered_map<fs::path, fs::file_time_type> lastWriteTimes;

	// Initialize the baseline.
	for (const auto& entry : fs::recursive_directory_iterator(m_watchDirectory))
	{
		if (IsValidEntry(entry))
		{
			PRINT_DEBUG("Watching file '{}'.", entry.path().string());

			lastWriteTimes[entry.path()] = entry.last_write_time();
		}
	}

	// Created in outer scope to avoid re-allocation each poll.
	std::vector<fs::path> validFileEntries;

	while (!m_shouldQuitThread)
	{
		validFileEntries.clear();

		m_changedFilesMutex.lock();

		for (const auto& entry : fs::recursive_directory_iterator(m_watchDirectory))
		{
			if (IsValidEntry(entry))
			{
				const fs::path& entryPath = entry.path();

				auto lastWriteIt = lastWriteTimes.find(entryPath);

				if (lastWriteIt != lastWriteTimes.end())
				{
					// If saved time is older than the current write time.
					if (lastWriteIt->second < entry.last_write_time())
					{
						PRINT_DEBUG("'{}' was updated.", entryPath.string());

						m_changedFiles.insert(entryPath);
					}
				}
				else
				{
					PRINT_DEBUG(
					    "New file '{}' was added to directory '{}'.",
					    fs::relative(entryPath, m_watchDirectory).string(),
					    m_watchDirectory.string()
					);

					m_changedFiles.insert(entryPath);
				}

				lastWriteTimes[entryPath] = entry.last_write_time();

				validFileEntries.push_back(entryPath);
			}
		}

		m_changedFilesMutex.unlock();

		// Clear all entries that dont exist in the directory anymore.
		std::erase_if(
		    lastWriteTimes,
		    [&validFileEntries, this](const std::pair<fs::path, fs::file_time_type>& pair)
		{
			bool hasBeenRemoved = !std::ranges::contains(validFileEntries, pair.first);

			if (hasBeenRemoved)
			{
				PRINT_DEBUG(
				    "File '{}' has been removed from directory '{}'.", pair.first.string(), m_watchDirectory.string()
				);
			}

			return hasBeenRemoved;
		}
		);

		std::this_thread::sleep_for(std::chrono::milliseconds(m_pollIntervalMS));
	}
}

bool DirectoryWatcher::IsValidEntry(const std::filesystem::directory_entry& dirEntry)
{
	bool hasValidExtension =
	    m_extensionFilters.empty() ? true : std::ranges::contains(m_extensionFilters, dirEntry.path().extension());

	return dirEntry.is_regular_file() && hasValidExtension;
}

DirectoryWatcher::DirectoryWatcher(
    const std::filesystem::path& watchDirectory,
    uint32_t pollInterval,
    const std::vector<std::string_view>& extensionFilters /*= {}*/
)
    : m_watchDirectory(watchDirectory), m_pollIntervalMS(pollInterval), m_extensionFilters(extensionFilters)
{
	m_watcherThread = std::thread(&DirectoryWatcher::WatchDirectory, this);
}

DirectoryWatcher::~DirectoryWatcher()
{
	m_shouldQuitThread = true;
	m_watcherThread.join();
}

void DirectoryWatcher::GetChangedFiles(std::unordered_set<std::filesystem::path>& changedFiles)
{
	ENSURE_EX(changedFiles.empty(), "Have to pass in an empty vector.");

	std::scoped_lock<std::mutex> scopedLock(m_changedFilesMutex);
	std::swap(changedFiles, m_changedFiles);
}
