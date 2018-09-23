//LogDownloader.hpp
//Author: Sivert Andresen Cubedo
#pragma once
#ifndef LogDownloader_HEADER
#define LogDownloader_HEADER

//C++
#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <chrono>

//boost
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/date_time.hpp>

//TLS
#include "root_certificates.hpp"

class Log
{
public:
	/*
	*/
	Log(const boost::posix_time::time_period & period, std::string && data);

	/*
	*/
	const boost::posix_time::time_period & getPeriod() const;

	/*
	*/
	const std::string & getSource() const;

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
	const std::vector<boost::posix_time::ptime> & getTimes() const;

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

	boost::posix_time::time_period m_period;
	
	std::vector<std::string_view> m_lines;
	std::vector<boost::posix_time::ptime> m_times;
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
		const boost::gregorian::greg_month & month,
		const boost::gregorian::greg_year & year
	);

	/*
	*/
	static std::string createGempirChannelTarget(
		const std::string_view & channel,
		const boost::gregorian::date & date
	);

	/*
	*/
	void run(
		CallbackType callback,
		const std::string & host,
		const std::string & port,
		const std::string & channel,
		const std::string & user,
		const boost::gregorian::greg_month & month,
		const boost::gregorian::greg_year & year,
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
	
	boost::posix_time::time_period m_period;

	std::string m_target;
	RequestType m_request;
	ResponseType m_response;

	CallbackType m_callback;
};

#endif // !LogDownloader_HEADER
