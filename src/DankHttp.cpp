//DankHttp.cpp

#include "../include/DankHttp.hpp"

namespace DankHttp
{
	NuulsUploader::NuulsUploader(boost::asio::io_context & ioc) :
		m_ioc(ioc),
		m_ctx{ boost::asio::ssl::context::sslv23_client },
		m_resolver(ioc)
	{
		load_root_certificates(m_ctx);
		m_stream_ptr = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(m_ioc, m_ctx);
	}

	std::string NuulsUploader::packBody(const std::string & data)
	{
		std::string body;
		//body.append(crlf);
		body.append("--").append(boundary).append(crlf);
		body.append("Content-Disposition: form-data; name=xddd; filename=xddd.png").append(crlf);
		body.append("Content-Type: text/plain").append(crlf);
		body.append(crlf);
		body.append(data).append(crlf);
		body.append("--").append(boundary).append("--");
		return body;
	}

	void NuulsUploader::run(CallbackType callback, const std::string & data, const std::string & host, const std::string & port, const std::string & target, int version)
	{
		m_host = host;
		m_port = port;
		m_target = target;
		m_version = version;
		m_callback = callback;

		if (!SSL_set_tlsext_host_name(m_stream_ptr->native_handle(), host.c_str())) {
			boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
			std::cerr << ec.message() << "\n";
			return;
		}

		std::string body = packBody(data);

		m_request.version(m_version);
		m_request.set(boost::beast::http::field::host, m_host);
		m_request.target(m_target);
		m_request.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
		m_request.method(boost::beast::http::verb::post);
		m_request.set(boost::beast::http::field::content_length, std::to_string(body.size()));
		m_request.set(boost::beast::http::field::content_type, std::string("multipart/form-data; boundary=").append(boundary));
		m_request.body() = std::move(body);

		m_resolver.async_resolve(
			m_host,
			m_port,
			std::bind(
				&NuulsUploader::resolveHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}

	void NuulsUploader::resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::asio::async_connect(
			m_stream_ptr->next_layer(),
			results.begin(),
			results.end(),
			std::bind(
				&NuulsUploader::connectHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	void NuulsUploader::connectHandler(boost::system::error_code ec)
	{
		if (ec) throw std::runtime_error(ec.message());
		m_stream_ptr->async_handshake(
			ssl::stream_base::client,
			std::bind(
				&NuulsUploader::handshakeHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	void NuulsUploader::handshakeHandler(boost::system::error_code ec)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::beast::http::async_write(
			*m_stream_ptr,
			m_request,
			std::bind(
				&NuulsUploader::writeHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}

	void NuulsUploader::writeHandler(boost::system::error_code ec, std::size_t bytes_transferred)
	{
		if (ec) throw std::runtime_error(ec.message());
		boost::ignore_unused(bytes_transferred);
		boost::beast::http::async_read(
			*m_stream_ptr, 
			m_buffer, 
			m_response,
			std::bind(
				&NuulsUploader::readHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2
			)
		);
	}

	void NuulsUploader::readHandler(boost::system::error_code ec, std::size_t bytes_transferred)
	{
		if (ec) throw std::runtime_error(ec.message());
		m_stream_ptr->async_shutdown(
			std::bind(
				&NuulsUploader::shutdownHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}

	void NuulsUploader::shutdownHandler(boost::system::error_code ec)
	{
		if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
		m_callback(std::move(m_response.body()));
	}




}