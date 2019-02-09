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
#include <limits>

//Date
#include <date/date.h>

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
#include "Log.hpp"

enum class LogService
{
	gempir_log,
	overrustle_log
};

struct LogRequest
{
	using CallbackType = std::function<void(Log&&)>;
	using ErrorHandlerType = std::function<void()>;
	using Target = std::pair<TimeDetail::TimePeriod, std::string>;
	using TargetIterator = std::vector<Target>::const_iterator;
	CallbackType callback;
	ErrorHandlerType error_handler;
	Log::ParserFunc parser;
	std::string host;
	std::string port;
	std::vector<Target> targets;
	int version = 11;
};

class LogDownloader : public std::enable_shared_from_this<LogDownloader>
{
public:
	using HttpRequestType = boost::beast::http::request<boost::beast::http::empty_body>;
	using HttpResponseType = boost::beast::http::response<boost::beast::http::string_body>;
	using HttpResponseParserType = boost::beast::http::response_parser<boost::beast::http::string_body>;

	LogDownloader(boost::asio::io_context & ioc);

	void run(LogRequest && request);

private:
	void errorHandler(boost::system::error_code ec);

	void resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

	void connectHandler(boost::system::error_code ec);

	void handshakeHandler(boost::system::error_code ec);

	void writeHandler(boost::system::error_code ec, std::size_t bytes_transferred, LogRequest::TargetIterator it);

	void readHandler(boost::system::error_code ec, std::size_t bytes_transferred, LogRequest::TargetIterator it);

	void shutdownHandler(boost::system::error_code ec);

	void fillHttpRequest(const LogRequest::Target & target);

	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ip::tcp::resolver m_resolver;
	std::optional<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_stream;
	boost::beast::flat_buffer m_buffer;
	HttpRequestType m_http_request;	
	std::optional<HttpResponseParserType> m_http_response_parser;

	std::mutex m_read_handler_mutex;
	LogRequest m_request;
};

/*
*/
std::string createGempirUserTarget(
	const std::string_view & channel,
	const std::string_view & user,
	const date::year_month & ym
);

/*
*/
std::string createGempirChannelTarget(
	const std::string_view & channel,
	const date::year_month_day & date
);

/*
*/
bool gempirLogParser(
	const std::string & data,
	std::vector<Log::LineView> & lines
);

/*
*/
std::string createOverrustleUserTarget(
	const std::string_view & channel,
	const std::string_view & user,
	const date::year_month & ym
);

/*
*/
std::string createOverrustleChannelTarget(
	const std::string_view & channel,
	const date::year_month_day & date
);

/*
*/
bool overrustleLogParser(
	const std::string & data,
	std::vector<Log::LineView> & lines
);

#if 0
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
	static bool parser(const std::string & data, std::vector<Log::LineView> & lines);

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
#endif

#endif // !LogDownloader_HEADER
