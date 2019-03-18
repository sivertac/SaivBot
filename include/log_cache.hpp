//log_cache.hpp
#pragma once
#ifndef log_cache_HEADER
#define log_cache_HEADER

//C++
#include <iostream>
#include <fstream>
#include <filesystem>
#include <list>
#include <unordered_map>
#include <optional>

//Date
#include <date/date.h>

//json
#include <nlohmann/json.hpp>

//Local
#include "TimeDetail.hpp"
#include "Log.hpp"
#include "lru_cache.hpp"

class log_tracker
{
public:
	log_tracker(std::filesystem::path && path) :
		m_path(std::move(path))
	{
	}

	log_tracker(std::filesystem::path && path, Log::Log && log) :
		m_path(std::move(path)),
		m_log(std::move(log))
	{
		if (!m_log->isValid()) throw std::runtime_error("invalid log");
		save_in_storage();
	}

	void load_in_memory()
	{
		std::ifstream ifs(m_path, std::ios::binary);
		if (!ifs.is_open()) throw std::runtime_error("Can't open file");
		m_log = Log::deserializeDefaultLogFormat(ifs);
		if (!m_log.has_value()) throw std::runtime_error("Error loading log");
	}

	void free_in_memory()
	{
		m_log.reset();
	}

	void save_in_storage()
	{
		//if file exist, then override
		std::ofstream ofs(m_path, std::ios::binary | std::ios::trunc);
		Log::serializeDefaultLogFormat(ofs, *m_log);
	}

	void free_in_storage()
	{
		//if file does not exist, then do nothing
		if (std::filesystem::exists(m_path)) {
			std::filesystem::remove(m_path);
		}
	}

	bool is_in_memory()
	{
		return m_log.has_value();
	}
	
private:
	std::string m_channel_name;
	TimeDetail::TimePeriod m_period;
	std::filesystem::path m_path;
	std::optional<Log::Log> m_log;
};

class log_cache
{
public:
	log_cache(std::filesystem::path && cache_dir) :
		m_cache_dir(std::move(cache_dir))
	{
	}

	void load_bookkeeping();

	void save_bookkeeping();

private:
	std::filesystem::path m_cache_dir;

	std::size_t m_memory_capacity;	//how much we're alloved to use in byte
	std::size_t m_memory_size;		//how much we are using in byte
	//lru_cache<TimeDetail::TimePeriod, std::reference_wrapper<log_tracker>, TimeDetail::TimePeriod::hash> m_memory_cache;

	std::size_t m_storage_capacity;	//how much we're alloved to use in byte
	std::size_t m_storage_size;		//how much we are using in byte
	//lru_cache<TimeDetail::TimePeriod, std::reference_wrapper<log_tracker>, TimeDetail::TimePeriod::hash> m_storage_cache;
};

#endif // !log_cache_HEADER
