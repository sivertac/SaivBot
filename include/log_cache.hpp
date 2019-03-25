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
	log_tracker(std::filesystem::path && path, Log::log_identifier && id) :
		m_path(std::move(path)),
		m_id(std::move(id))
	{
		if (!std::filesystem::exists(m_path)) throw std::runtime_error("File does not exist");
		m_in_storage_size = std::filesystem::file_size(m_path);
	}

	log_tracker(std::filesystem::path && path, Log::Log && log) :
		m_path(std::move(path)),	
		m_log(std::make_shared<const Log::Log>(std::move(log))),
		m_id(m_log->get_id())
	{
		save_in_storage();
	}

	void load_in_memory()
	{
		std::ifstream ifs(m_path, std::ios::binary);
		if (!ifs.is_open()) throw std::runtime_error("Can't open file");
		auto log = Log::deserializeDefaultLogFormat(ifs);
		if (log) {
			m_log = std::make_shared<const Log::Log>(std::move(*log));
		}
		else {
			throw std::runtime_error("Error loading log");
		}
		m_in_memory_size = log->compute_size_byte();
	}

	void free_in_memory()
	{
		m_log.reset();
		m_in_memory_size = 0;
	}

	void save_in_storage()
	{
		//if file exist, then override
		std::ofstream ofs(m_path, std::ios::binary | std::ios::trunc);
		Log::serializeDefaultLogFormat(ofs, *m_log);
		ofs.close();
		m_in_storage_size = std::filesystem::file_size(m_path);
	}

	void free_in_storage()
	{
		//if file does not exist, then do nothing
		if (std::filesystem::exists(m_path)) {
			std::filesystem::remove(m_path);
		}
		m_in_storage_size = 0;
	}

	bool is_in_memory()
	{
		//return m_log.has_value();
		return m_log.operator bool();
	}

	//can only be computed if the log is in memory, else return 0
	std::size_t in_memory_size();

	//can only be computed if the log is in storage, else return 0
	std::size_t in_storage_size();

	const Log::log_identifier & get_id() const noexcept
	{
		return m_id;
	}

	std::shared_ptr<const Log::Log> get_log() const noexcept
	{
		return m_log;
	}

	friend void to_json(nlohmann::json & j, const log_tracker & t)
	{
		j = nlohmann::json{ {"path", t.m_path.string()}, {"id", t.m_id} };
	}
	
	friend void from_json(const nlohmann::json & j, log_tracker & t)
	{
		std::string p;
		j.at("path").get_to(p);
		t.m_path = std::filesystem::path(p);
		j.at("id").get_to(t.m_id);
	}

private:
	std::filesystem::path m_path;
	std::shared_ptr<const Log::Log> m_log;
	Log::log_identifier m_id;
	std::size_t m_in_memory_size = 0;
	std::size_t m_in_storage_size = 0;
};

class log_cache
{
public:
	log_cache(boost::asio::io_context & ioc) :
		m_ioc(ioc)
	{
	}

	/*
	log_cache(std::filesystem::path && cache_dir, std::filesystem::path && bookkeeping_path) :
		m_cache_dir(std::forward<std::filesystem::path>(cache_dir)),
		m_bookkeeping_path(std::forward<std::filesystem::path>(bookkeeping_path))
	{
	}
	*/

	void put(Log::Log && log)
	{
		log_tracker tracker("", std::move(log));
		m_memory_cache.put(tracker.get_id(), std::move(tracker));
	}

	std::shared_ptr<const Log::Log> get(const Log::log_identifier & key)
	{
		auto it = m_memory_cache.get(key);
		if (it != m_memory_cache.end()) {
			return it->second.get_log();
		}
		else {
			return nullptr;
		}
	}

	//void load_bookkeeping()
	//{
	//	std::ifstream ss(m_bookkeeping_path);
	//	if (!ss.is_open()) throw std::runtime_error("Can't open m_bookkeeping_path");
	//	nlohmann::json j;
	//	ss >> j;
	//}
	//
	//void save_bookkeeping()
	//{
	//
	//}
private:
	boost::asio::io_context & m_ioc;

	//const std::filesystem::path m_bookkeeping_path;
	//const std::filesystem::path m_cache_dir;

	std::size_t m_memory_capacity;	//how much we're alloved to use in byte
	std::size_t m_memory_size;		//how much we are using in byte
	lru_cache<Log::log_identifier, log_tracker, Log::log_identifier::hash> m_memory_cache;

	//std::size_t m_storage_capacity;	//how much we're alloved to use in byte
	//std::size_t m_storage_size;		//how much we are using in byte
	//lru_cache<TimeDetail::TimePeriod, std::reference_wrapper<log_tracker>, TimeDetail::TimePeriod::hash> m_storage_cache;
};

#endif // !log_cache_HEADER
