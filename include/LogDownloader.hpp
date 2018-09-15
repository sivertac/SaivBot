//LogDownloader.hpp
//Author: Sivert Andresen Cubedo
#pragma once
#ifndef LogDownloader_HEADER
#define LogDownloader_HEADER

#define _WIN32_WINNT 0x0A00

//C++
#include <iostream>
#include <string>
#include <memory>
#include <functional>
#include <optional>
#include <unordered_map>
#include <mutex>

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

class Log
{
public:
	using LineContainer = std::vector<std::tuple<std::string_view, std::string_view, std::string_view>>;

	/*
	*/
	Log(std::string && source, std::string && data);

	/*
	*/
	const std::string & getSource() const;

	/*
	*/
	const std::string & getData() const;

	/*
	*/
	const LineContainer & getLines() const;
private:
	/*
	*/
	void parse();

	std::string m_source;
	std::string m_data;

	//0: whole line, 1: prefix, 2: message
	LineContainer m_lines;
};

class LogDownloader : public std::enable_shared_from_this<LogDownloader>
{
public:
	using LogContainer = std::vector<Log>;
	using CallbackType = std::function<void(LogContainer&&)>;

	using RequestType = boost::beast::http::request<boost::beast::http::empty_body>;
	using ResponseType = boost::beast::http::response<boost::beast::http::string_body>;

	/*
	Resolver and stream require an io_context
	*/
	LogDownloader(
		boost::asio::io_context& ioc
	) :
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
		std::vector<std::string> && targets,
		int version = 11
	)
	{
		m_host = host;
		m_port = port;
		m_version = version;
		m_targets = std::move(targets);
		m_callback = callback;

		if (!SSL_set_tlsext_host_name(m_stream_ptr->native_handle(), host.c_str())) {
			boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			std::cerr << ec.message() << "\n";
			return;
		}

		//m_req.version(version);
		//m_req.method(boost::beast::http::verb::get);
		//m_req.target(target);
		//m_req.set(boost::beast::http::field::host, host);
		//m_req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

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

		m_target_counter = m_targets.size();
		m_requests.resize(m_targets.size());
		m_responses.resize(m_targets.size());
		for (std::size_t i = 0; i < m_requests.size(); ++i) {
			RequestType & req = m_requests[i];
			req.version(m_version);
			req.method(boost::beast::http::verb::get);
			req.target(m_targets[i]);
			req.set(boost::beast::http::field::host, m_host);
			req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

			boost::beast::http::async_write(
				*m_stream_ptr,
				req,
				std::bind(
					&LogDownloader::writeHandler,
					shared_from_this(),
					std::placeholders::_1,
					std::placeholders::_2,
					m_responses.begin() + i
				)
			);
		}
	}

	/*
	*/
	void writeHandler(boost::system::error_code ec, std::size_t bytes_transferred, std::vector<ResponseType>::iterator res_it)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::ignore_unused(bytes_transferred);
		boost::beast::http::async_read(*m_stream_ptr, m_buffer, *res_it,
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
		std::lock_guard<std::mutex> lock(m_target_counter_mutex);
		
		assert(m_target_counter > 0);
		--m_target_counter;

		if (m_target_counter == 0) {
			boost::ignore_unused(bytes_transferred);
			m_stream_ptr->async_shutdown(
				std::bind(
					&LogDownloader::shutdownHandler,
					shared_from_this(),
					std::placeholders::_1
				)
			);
		}
		if (ec) throw std::runtime_error(ec.message());
	}

	/*
	*/
	void shutdownHandler(boost::system::error_code ec)
	{
		if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
		
		LogContainer logs;
		logs.reserve(m_targets.size());
		for (std::size_t i = 0; i < m_targets.size(); ++i) {
			logs.emplace_back(std::move(m_targets[i]), std::move(m_responses[i].body()));
		}

		m_callback(std::move(logs));
	}

private:

	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ip::tcp::resolver m_resolver;
	std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_stream_ptr;
	boost::beast::flat_buffer m_buffer;

	//boost::beast::http::request<boost::beast::http::empty_body> m_req;
	//boost::beast::http::response<boost::beast::http::string_body> m_res;

	std::string m_host;
	std::string m_port;
	int m_version;
	
	std::vector<std::string> m_targets;
	std::vector<RequestType> m_requests;
	std::vector<ResponseType> m_responses;

	std::mutex m_target_counter_mutex;
	std::size_t m_target_counter;

	//std::string m_source;

	CallbackType m_callback;
};

#endif // !LogDownloader_HEADER
