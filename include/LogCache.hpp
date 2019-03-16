//LogCache.hpp
#pragma once
#ifndef LogCache_HEADER
#define LogCache_HEADER

//C++
#include <filesystem>
#include <list>
#include <unordered_map>
#include <optional>

//Date
#include <date/date.h>

//Local
#include "TimeDetail.hpp"
#include "Log.hpp"

class LogTracker
{
public:
	LogTracker(std::filesystem::path && path);
	LogTracker(Log::Log && log);

	void loadInMemory();

	void freeInMemory();

	void saveInStorage();

	void freeInStorage();

	bool isInMemory();

	bool isInStorage();
private:
	std::string m_channel_name;
	TimeDetail::TimePeriod m_period;
	std::filesystem::path m_path;
	std::optional<Log::Log> m_log;
};

class LogCache
{
public:
	LogCache(std::filesystem::path && cache_dir);



private:
	std::filesystem::path m_cache_dir;



	std::vector<LogTracker> m_in_memory_logs;
	std::vector<LogTracker> m_in_storage_logs;
};

#endif // !LogCache_HEADER
