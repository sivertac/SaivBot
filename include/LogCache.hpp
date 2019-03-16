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
#include "lru_cache.hpp"

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
	bool m_in_storage = false;
	std::optional<Log::Log> m_log;
};

class LogCache
{
public:
	LogCache(std::filesystem::path && cache_dir) :
		m_cache_dir(std::move(cache_dir))
	{
	}



private:
	std::filesystem::path m_cache_dir;

	std::size_t m_memory_capacity;	//how much we're alloved to use in byte
	std::size_t m_memory_size;		//how much we are using in byte
	lru_cache<TimeDetail::TimePeriod, std::reference_wrapper<LogTracker>, TimeDetail::TimePeriod::hash> m_memory_cache;

	std::size_t m_storage_capacity;	//how much we're alloved to use in byte
	std::size_t m_storage_size;		//how much we are using in byte
	lru_cache<TimeDetail::TimePeriod, std::reference_wrapper<LogTracker>, TimeDetail::TimePeriod::hash> m_storage_cache;

};

#endif // !LogCache_HEADER
