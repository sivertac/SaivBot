//LogDownloader.hpp
//Author: Sivert Andresen Cubedo
#pragma once
#ifndef LogDownloader_HEADER
#define LogDownloader_HEADER

//C++
#include <iostream>
#include <string>
#include <memory>
#include <array>
#include <functional>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <chrono>

//Date
#include <date\date.h>

//boost
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
//#include <boost/date_time.hpp>

//TLS
#include "root_certificates.hpp"

namespace TimeDetail
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
	
	inline std::string monthToString(date::month month)
	{
		std::stringstream ss;
		ss << date::format("%B", month);
		return ss.str();
	}

	inline std::optional<TimePoint> parseTimeString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		TimePoint point;
		ss >> date::parse("%F %T", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::year_month_day> parseDateString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::year_month_day point;
		ss >> date::parse("%F", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::month> parseMonthString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::month point;
		ss >> date::parse("%B", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::year> parseYearString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::year point;
		ss >> date::parse("%g", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<TimePoint> parseTimePointString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		std::chrono::system_clock::time_point point;
		if (!(ss >> date::parse("%Y-%m-%d-%H-%M-%S", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d-%H-%M", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d-%H", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		date::year_month year_month;
		if (!(ss >> date::parse("%Y-%m", year_month)).fail()) {
			return date::sys_days(year_month / 1);
		}
		ss = std::stringstream();
		ss << view;
		date::year year;
		if (!(ss >> date::parse("%Y", year)).fail()) {
			return date::sys_days(year / 1 / 1);
		}
		return std::nullopt;
	}

	class TimePeriod
	{
	public:
		TimePeriod() = default;
		TimePeriod(TimePoint begin, TimePoint end) :
			m_begin(begin),
			m_end(end)
		{
			assert(begin <= end);
		}

		bool isInside(TimePoint point) const
		{
			if (point >= m_begin && point < m_end) {
				return true;
			}
			else {
				return false;
			}
		}

		TimePoint begin() const
		{
			return m_begin;
		}

		TimePoint end() const
		{
			return m_end;
		}
	private:
		TimePoint m_begin;
		TimePoint m_end;
	};

	inline std::optional<TimePeriod> parseTimePeriod(const std::string_view & begin, const std::string_view & end)
	{
		TimePoint begin_time;
		TimePoint end_time;
		if (auto r = parseTimePointString(begin)) {
			begin_time = *r;
		}
		else {
			return std::nullopt;
		}
		if (auto r = parseTimePointString(end)) {
			end_time = *r;
		}
		else {
			return std::nullopt;
		}
		if (begin_time > end_time) {
			return std::nullopt;
		}
		return TimePeriod(begin_time, end_time);
	}
};

class Log
{
public:
	/*
	*/
	Log(TimeDetail::TimePeriod period, std::string && data);

	/*
	*/
	TimeDetail::TimePeriod getPeriod() const;

	/*
	*/
	const std::string & getData() const;

	/*
	*/
	const std::vector<std::string_view> & getLines() const;

	/*
	*/
	std::size_t getNumberOfLines() const;

	/*
	*/
	const std::vector<TimeDetail::TimePoint> & getTimes() const;

	/*
	*/
	const std::vector<std::string_view> & getNames() const;

	/*
	*/
	const std::vector<std::string_view> & getMessages() const;
private:
	/*
	*/
	void parse();

	std::string m_data;

	const TimeDetail::TimePeriod m_period;

	std::vector<std::string_view> m_lines;
	std::vector<TimeDetail::TimePoint> m_times;
	std::vector<std::string_view> m_names;
	std::vector<std::string_view> m_messages;
};

class GempirUserLogDownloader : public std::enable_shared_from_this<GempirUserLogDownloader>
{
public:
	using CallbackType = std::function<void(Log&&)>;
	using RequestType = boost::beast::http::request<boost::beast::http::empty_body>;
	using ResponseType = boost::beast::http::response<boost::beast::http::string_body>;

	/*
	Resolver and stream require an io_context.
	*/
	GempirUserLogDownloader(boost::asio::io_context & ioc);

	/*
	*/
	static std::string createGempirUserTarget(
		const std::string_view & channel,
		const std::string_view & user,
		const date::month & month,
		const date::year & year
	);

	/*
	*/
	static std::string createGempirChannelTarget(
		const std::string_view & channel,
		const date::year_month_day & date
	);

	/*
	*/
	void run(
		CallbackType callback,
		const std::string & host,
		const std::string & port,
		const std::string & channel,
		const std::string & user,
		const date::month & month,
		const date::year & year,
		int version = 11
	);

	/*
	*/
	void resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

	/*
	*/
	void connectHandler(boost::system::error_code ec);

	/*
	*/
	void handshakeHandler(boost::system::error_code ec);
	
	/*
	*/
	void writeHandler(boost::system::error_code ec, std::size_t bytes_transferred);

	/*
	*/
	void readHandler(boost::system::error_code ec, std::size_t bytes_transferred);

	/*
	*/
	void shutdownHandler(boost::system::error_code ec);

private:
	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ip::tcp::resolver m_resolver;
	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_stream_ptr;
	boost::beast::flat_buffer m_buffer;

	int m_version;
	std::string m_host;
	std::string m_port;

	std::string m_channel;
	std::string m_user;
	
	TimeDetail::TimePeriod m_period;

	std::string m_target;
	RequestType m_request;
	ResponseType m_response;

	CallbackType m_callback;
};

#endif // !LogDownloader_HEADER
