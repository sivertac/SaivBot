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

//TLS
#include "root_certificates.hpp"

//local
#include "TimeDetail.hpp"

class Log
{
public:
	class LineView
	{
	public:
		const std::string_view & getLineView() const
		{
			return m_line_view;
		}
		const std::string_view & getTimeView() const
		{
			return m_time_view;
		}
		const TimeDetail::TimePoint & getTime() const
		{
			return m_time;
		}
		const std::string_view & getNameView() const
		{
			return m_name_view;
		}
		const std::string_view & getMessageView() const
		{
			return m_message_view;
		}
	private:
		friend Log;
		std::string_view m_line_view;
		std::string_view m_time_view;
		TimeDetail::TimePoint m_time;
		std::string_view m_name_view;
		std::string_view m_message_view;
	};

	/*
	*/
	Log(TimeDetail::TimePeriod period, std::string && data);

	/*
	*/
	bool isValid() const;

	/*
	*/
	TimeDetail::TimePeriod getPeriod() const;

	/*
	*/
	const std::string & getData() const;

	/*
	*/
	const std::vector<LineView> & getLines() const;

	/*
	*/
	std::size_t getNumberOfLines() const;

private:
	/*
	*/
	void parse();

	bool m_valid = false;
	std::string m_data;

	const TimeDetail::TimePeriod m_period;

	std::vector<LineView> m_lines;
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
