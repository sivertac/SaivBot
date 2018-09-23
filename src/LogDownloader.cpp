//GempirUserLogDownloader.cpp

#include "../include/LogDownloader.hpp"

Log::Log(const boost::posix_time::time_period & period, std::string && data) :
	m_period(period),
	m_data(std::move(data))
{
	parse();
}

const boost::posix_time::time_period & Log::getPeriod() const
{
	return m_period;
}

const std::string & Log::getData() const
{
	return m_data;
}

const std::vector<std::string_view> & Log::getLines() const
{
	return m_lines;
}

std::size_t Log::getNumberOfLines() const
{
	return m_lines.size();
}

const std::vector<boost::posix_time::ptime>& Log::getTimes() const
{
	return m_times;
}

const std::vector<std::string_view>& Log::getNames() const
{
	return m_names;
}

const std::vector<std::string_view>& Log::getMessages() const
{
	return m_messages;
}

void Log::parse()
{
	const std::string_view cr("\r\n");
	std::string_view data_view(m_data);
	while (!data_view.empty()) {
		auto cr_pos = data_view.find(cr);
		if (cr_pos == data_view.npos) break;
		std::string_view line = data_view.substr(0, cr_pos + cr.size());
		std::string_view all = data_view.substr(0, cr_pos);

		auto space = all.find("] ");
		if (space == all.npos) break;

		std::string_view time = all.substr(1, space - 1);
		all.remove_prefix(space + 2);

		space = all.find(' ');
		if (space == all.npos) break;
		std::string_view name = all.substr(0, space - 1);

		all.remove_prefix(space + 1);
		std::string_view message = all;

		m_lines.push_back(line);
		m_times.push_back(boost::posix_time::time_from_string(std::string(time)));
		m_names.push_back(name);
		m_messages.push_back(message);

		//advance
		data_view.remove_prefix(cr_pos + cr.size());
	}
}

GempirUserLogDownloader::GempirUserLogDownloader(boost::asio::io_context & ioc) :
	m_ioc(ioc),
	m_ctx{ boost::asio::ssl::context::sslv23_client },
	m_resolver(ioc),
	m_period(boost::posix_time::ptime(), boost::posix_time::ptime())
{
	load_root_certificates(m_ctx);
	m_stream_ptr = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(m_ioc, m_ctx);
}

std::string GempirUserLogDownloader::createGempirUserTarget(const std::string_view & channel, const std::string_view & user, const boost::gregorian::greg_month & month, const boost::gregorian::greg_year & year)
{
	std::string target;

	target.append("/channel/");
	target.append(channel);
	target.append("/user/");
	target.append(user);
	target.append("/");
	target.append(std::to_string(year));
	target.append("/");
	target.append(month.as_long_string());

	return target;
}

std::string GempirUserLogDownloader::createGempirChannelTarget(const std::string_view & channel, const boost::gregorian::date & date)
{
	std::string target;

	target.append("/channel/");
	target.append(channel);
	target.append("/");
	target.append(std::to_string(date.year()));
	target.append("/");
	target.append(date.month().as_long_string());
	target.append("/");
	target.append(std::to_string(date.day()));

	return target;
}

void GempirUserLogDownloader::run(CallbackType callback, const std::string & host, const std::string & port, const std::string & channel, const std::string & user, const boost::gregorian::greg_month & month, const boost::gregorian::greg_year & year, int version)
{
	m_version = version;
	m_host = host;
	m_port = port;
	m_target = createGempirUserTarget(channel, user, month, year);
	m_callback = callback;

	m_channel = channel;
	m_user = user;

	//period
	{
		boost::gregorian::date date_begin(year, month, 1);
		boost::gregorian::date date_end(date_begin + boost::gregorian::months(1));
		m_period = boost::posix_time::time_period(boost::posix_time::ptime(date_begin), boost::posix_time::ptime(date_end));
	}

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
			&GempirUserLogDownloader::resolveHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void GempirUserLogDownloader::resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec) throw std::runtime_error(ec.message());
	boost::asio::async_connect(
		m_stream_ptr->next_layer(),
		results.begin(),
		results.end(),
		std::bind(
			&GempirUserLogDownloader::connectHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void GempirUserLogDownloader::connectHandler(boost::system::error_code ec)
{
	if (ec) throw std::runtime_error(ec.message());
	m_stream_ptr->async_handshake(
		ssl::stream_base::client,
		std::bind(
			&GempirUserLogDownloader::handshakeHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void GempirUserLogDownloader::handshakeHandler(boost::system::error_code ec)
{
	if (ec) throw std::runtime_error(ec.message());

	boost::beast::http::async_write(
		*m_stream_ptr,
		m_request,
		std::bind(
			&GempirUserLogDownloader::writeHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void GempirUserLogDownloader::writeHandler(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec) throw std::runtime_error(ec.message());
	boost::ignore_unused(bytes_transferred);
	boost::beast::http::async_read(*m_stream_ptr, m_buffer, m_response,
		std::bind(
			&GempirUserLogDownloader::readHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void GempirUserLogDownloader::readHandler(boost::system::error_code ec, std::size_t bytes_transferred)
{
	boost::ignore_unused(bytes_transferred);
	m_stream_ptr->async_shutdown(
		std::bind(
			&GempirUserLogDownloader::shutdownHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
	if (ec) throw std::runtime_error(ec.message());
}

void GempirUserLogDownloader::shutdownHandler(boost::system::error_code ec)
{
	if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
	Log log(m_period, std::move(m_response.body()));
	m_callback(std::move(log));
}
