//DankHttp.hpp
#pragma once
#ifndef DankHttp_HEADER
#define DankHttp_HEADER

//C++
#include <iostream>
#include <string>
#include <memory>

//boost
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

//TLS
#include "root_certificates.hpp"

namespace DankHttp
{
	class Uploader : public std::enable_shared_from_this<Uploader>
	{
	public:
		using CallbackType = std::function<void(std::string&&)>;
		using RequestType = boost::beast::http::request<boost::beast::http::string_body>;
		using ResponseType = boost::beast::http::response<boost::beast::http::string_body>;

		/*
		*/
		Uploader(boost::asio::io_context & ioc);

		/*
		*/
		void run(
			CallbackType callback,
			std::string && data, 
			const std::string & host,
			const std::string & port,
			const std::string & target,
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
		boost::beast::flat_buffer m_buffer;

		std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_stream_ptr;
		
		std::string m_host;
		std::string m_port;
		std::string m_target;
		int m_version;

		RequestType m_request;
		ResponseType m_response;

		//std::string m_upload_data;
		CallbackType m_callback;
	};





}

#endif // !DankHttp_HEADER
