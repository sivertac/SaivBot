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
	const boost::posix_time::time_period & getPeriod();

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

class LogDownloader : public std::enable_shared_from_this<LogDownloader>
{
public:
	using CallbackType = std::function<void(Log&&)>;

	using RequestType = boost::beast::http::request<boost::beast::http::empty_body>;
	using ResponseType = boost::beast::http::response<boost::beast::http::string_body>;

	/*
	Resolver and stream require an io_context
	*/
	LogDownloader(boost::asio::io_context & ioc) :
		m_ioc(ioc),
		m_ctx{ boost::asio::ssl::context::sslv23_client },
		m_resolver(ioc)
	{
		load_root_certificates(m_ctx);
		m_stream_ptr = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(m_ioc, m_ctx);
	}

	/*
	*/
	void run(
		CallbackType callback,
		const std::string & host,
		const std::string & port,
		const std::string & target,
		int version = 11
	)
	{
		m_host = host;
		m_port = port;
		m_version = version;
		m_target = target;
		m_callback = callback;

		if (!SSL_set_tlsext_host_name(m_stream_ptr->native_handle(), host.c_str())) {
			boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			std::cerr << ec.message() << "\n";
			return;
		}

		m_request.version(m_version);
		m_request.method(boost::beast::http::verb::get);
		m_request.target(m_target);
		m_request.set(boost::beast::http::field::host, m_host);
		m_request.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

		m_resolver.async_resolve(
			host,
			port,
			std::bind(
				&LogDownloader::resolveHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}

	/*
	*/
	void resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::asio::async_connect(
			m_stream_ptr->next_layer(),
			results.begin(),
			results.end(),
			std::bind(
				&LogDownloader::connectHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	/*
	*/
	void connectHandler(boost::system::error_code ec)
	{
		if (ec) throw std::runtime_error(ec.message());
		m_stream_ptr->async_handshake(
			ssl::stream_base::client,
			std::bind(
				&LogDownloader::handshakeHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	/*
	*/
	void handshakeHandler(boost::system::error_code ec)
	{
		if (ec) throw std::runtime_error(ec.message());

		boost::beast::http::async_write(
			*m_stream_ptr,
			m_request,
			std::bind(
				&LogDownloader::writeHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}
	

	/*
	*/
	void writeHandler(boost::system::error_code ec, std::size_t bytes_transferred)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::ignore_unused(bytes_transferred);
		boost::beast::http::async_read(*m_stream_ptr, m_buffer, m_response,
			std::bind(
				&LogDownloader::readHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}

	/*
	*/
	void readHandler(boost::system::error_code ec, std::size_t bytes_transferred)
	{
		boost::ignore_unused(bytes_transferred);
		m_stream_ptr->async_shutdown(
			std::bind(
				&LogDownloader::shutdownHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
		if (ec) throw std::runtime_error(ec.message());
	}

	/*
	*/
	void shutdownHandler(boost::system::error_code ec)
	{
		if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
		boost::posix_time::ptime start(boost::gregorian::date(1996, 8, 24));
		boost::posix_time::ptime end(boost::gregorian::date(2018, 8, 24));
		Log log(boost::posix_time::time_period(start, end), std::move(m_response.body()));
		m_callback(std::move(log));
	}

private:

	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ip::tcp::resolver m_resolver;
	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_stream_ptr;
	boost::beast::flat_buffer m_buffer;

	std::string m_host;
	std::string m_port;
	int m_version;
	
	std::string m_target;
	RequestType m_request;
	ResponseType m_response;

	CallbackType m_callback;
};

#endif // !LogDownloader_HEADER
