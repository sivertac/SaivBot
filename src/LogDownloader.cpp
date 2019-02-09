//GempirUserLogDownloader.cpp

#include "../include/LogDownloader.hpp"

#if 0
GempirUserLogDownloader::GempirUserLogDownloader(boost::asio::io_context & ioc) :
	m_ioc(ioc),
	m_ctx{ boost::asio::ssl::context::sslv23_client },
	m_resolver(ioc)
{
	load_root_certificates(m_ctx);
	m_stream_ptr = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(m_ioc, m_ctx);
}

std::string GempirUserLogDownloader::createGempirUserTarget(const std::string_view & channel, const std::string_view & user, const date::month & month, const date::year & year)
{
	std::string target;

	target.append("/channel/");
	target.append(channel);
	target.append("/user/");
	target.append(user);
	target.append("/");
	target.append(std::to_string(static_cast<int>(year)));
	target.append("/");
	target.append(TimeDetail::monthToString(month));

	return target;
}

std::string GempirUserLogDownloader::createGempirChannelTarget(const std::string_view & channel, const date::year_month_day & date)
{
	std::string target;

	target.append("/channel/");
	target.append(channel);
	target.append("/");
	target.append(std::to_string(static_cast<int>(date.year())));
	target.append("/");
	target.append(TimeDetail::monthToString(date.month()));
	target.append("/");
	target.append(std::to_string(static_cast<unsigned int>(date.day())));

	return target;
}

void GempirUserLogDownloader::run(CallbackType callback, const std::string & host, const std::string & port, const std::string & channel, const std::string & user, const date::month & month, const date::year & year, int version)
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
		auto date_begin = date::year_month_day(year, month, date::day(1));
		auto date_end = date_begin + date::months(1);
		TimeDetail::TimePoint time_begin = date::sys_days(date_begin);
		TimeDetail::TimePoint time_end = date::sys_days(date_end);
		m_period = TimeDetail::TimePeriod(time_begin, time_end);
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
	if (ec) throw std::runtime_error(ec.message());
	boost::ignore_unused(bytes_transferred);
	m_stream_ptr->async_shutdown(
		std::bind(
			&GempirUserLogDownloader::shutdownHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void GempirUserLogDownloader::shutdownHandler(boost::system::error_code ec)
{
	if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
	Log log(m_period, std::move(m_response.body()), parser);
	m_callback(std::move(log));
}

bool GempirUserLogDownloader::parser(const std::string & data, std::vector<Log::LineView>& lines)
{
	const std::string_view cr("\r\n");
	std::string_view data_view(data);
	while (!data_view.empty()) {
		auto cr_pos = data_view.find(cr);
		if (cr_pos == data_view.npos) break;
		std::string_view line_view = data_view.substr(0, cr_pos + cr.size());
		std::string_view all_view = data_view.substr(0, cr_pos);

		auto space = all_view.find("] ");
		if (space == all_view.npos) break;

		std::string_view time_view = all_view.substr(1, space - 1);
		all_view.remove_prefix(space + 2);

		space = all_view.find(' ');
		if (space == all_view.npos) break;
		std::string_view name_view = all_view.substr(0, space - 1);

		all_view.remove_prefix(space + 1);
		std::string_view message_view = all_view;

		TimeDetail::TimePoint time;

		if (auto r = TimeDetail::parseTimeString(time_view)) {
			time = *r;
		}
		else {
			return false;
		}

		lines.emplace_back(
			line_view,
			time_view,
			time,
			name_view,
			message_view
		);

		//advance
		data_view.remove_prefix(cr_pos + cr.size());
	}
	return true;
}
#endif

LogDownloader::LogDownloader(boost::asio::io_context & ioc) :
	m_ioc(ioc),
	m_ctx{ boost::asio::ssl::context::sslv23_client },
	m_resolver(ioc)
{
	load_root_certificates(m_ctx);
	m_stream.emplace(m_ioc, m_ctx);
	m_http_response_parser.emplace();
	m_http_response_parser->body_limit(std::numeric_limits<std::uint64_t>::max());
}

void LogDownloader::run(LogRequest && request)
{
	m_request = std::move(request);

	if (!SSL_set_tlsext_host_name(m_stream->native_handle(), m_request.host.c_str())) {
		boost::system::error_code ec{ static_cast<int>(::ERR_get_error()), boost::asio::error::get_ssl_category() };
		std::cerr << ec.message() << "\n";
		return;
	}

	m_resolver.async_resolve(
		m_request.host,
		m_request.port,
		std::bind(
			&LogDownloader::resolveHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

void LogDownloader::errorHandler(boost::system::error_code ec)
{
	m_request.error_handler();
	std::stringstream ss;
	ss << "LogDownloader error: " << ec.message();
	throw std::runtime_error(ss.str());
}

void LogDownloader::resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec) {
		errorHandler(ec);
	}
	boost::asio::async_connect(
		m_stream->next_layer(),
		results.begin(),
		results.end(),
		std::bind(
			&LogDownloader::connectHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void LogDownloader::connectHandler(boost::system::error_code ec)
{
	if (ec) {
		errorHandler(ec);
	}
	m_stream->async_handshake(
		ssl::stream_base::client,
		std::bind(
			&LogDownloader::handshakeHandler,
			shared_from_this(),
			std::placeholders::_1
		)
	);
}

void LogDownloader::handshakeHandler(boost::system::error_code ec)
{
	if (ec) {
		errorHandler(ec);
	}
	auto it = m_request.targets.begin();
	fillHttpRequest(*it);
	boost::beast::http::async_write(
		*m_stream,
		m_http_request,
		std::bind(
			&LogDownloader::writeHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2,
			it
		)
	);
}

void LogDownloader::writeHandler(boost::system::error_code ec, std::size_t bytes_transferred, LogRequest::TargetIterator it)
{
	if (ec) {
		errorHandler(ec);
	}
	boost::ignore_unused(bytes_transferred);
	boost::beast::http::async_read(
		*m_stream, 
		m_buffer,	
		*m_http_response_parser,
		std::bind(
			&LogDownloader::readHandler,
			shared_from_this(),
			std::placeholders::_1,
			std::placeholders::_2,
			it
		)
	);
}

void LogDownloader::readHandler(boost::system::error_code ec, std::size_t bytes_transferred, LogRequest::TargetIterator it)
{
	if (ec) {
		errorHandler(ec);
	}
	boost::ignore_unused(bytes_transferred);

	std::lock_guard<std::mutex> lock(m_read_handler_mutex);
	
	std::string temp_data = std::move(m_http_response_parser->get().body());
	m_http_response_parser.emplace();
	m_http_response_parser->body_limit(std::numeric_limits<std::uint64_t>::max());
	
	auto next_it = std::next(it);
	
	if (next_it != m_request.targets.cend()) {
		fillHttpRequest(*next_it);
		boost::beast::http::async_write(
			*m_stream,
			m_http_request,
			std::bind(
				&LogDownloader::writeHandler,
				shared_from_this(),
				std::placeholders::_1,
				std::placeholders::_2,
				next_it
			)
		);
	}
	else {
		m_stream->async_shutdown(
			std::bind(
				&LogDownloader::shutdownHandler,
				shared_from_this(),
				std::placeholders::_1
			)
		);
	}
	
	Log log(std::move(std::get<0>(*it)), std::move(std::get<1>(*it)), std::move(temp_data), m_request.parser);

	m_request.callback(std::move(log));
}

void LogDownloader::shutdownHandler(boost::system::error_code ec)
{
	if (ec && ec.value() != boost::asio::ssl::error::stream_truncated) throw std::runtime_error(ec.message());
	//if (ec) throw std::runtime_error(ec.message());
}

void LogDownloader::fillHttpRequest(const LogRequest::Target & target)
{
	m_http_request = HttpRequestType();
	m_http_request.version(m_request.version);
	m_http_request.method(boost::beast::http::verb::get);
	m_http_request.target(std::get<2>(target));
	m_http_request.set(boost::beast::http::field::host, m_request.host);
	m_http_request.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);
}

std::string createGempirUserTarget(const std::string_view & channel, const std::string_view & user, const date::year_month & ym)
{
	std::stringstream target;
	target 
		<< "/channel/"
		<< channel
		<< "/user/"
		<< user
		<< "/"
		<< std::to_string(static_cast<int>(ym.year()))
		<< "/"
		<< std::to_string(static_cast<unsigned int>(ym.month()));
	return target.str();
}

std::string createGempirChannelTarget(const std::string_view & channel, const date::year_month_day & date)
{
	//https://api.gempir.com/channel/pajlada/2019/2/8
	std::stringstream target;
	target
		<< "/channel/"
		<< channel
		<< "/"
		<< std::to_string(static_cast<int>(date.year()))
		<< "/"
		<< std::to_string(static_cast<unsigned int>(date.month()))
		<< "/"
		<< std::to_string(static_cast<unsigned int>(date.day()));
	return target.str();
}

bool gempirLogParser(const std::string & data, std::vector<Log::LineView>& lines)
{
	if (data == "{\"message\":\"Failure reading log\"}") return false;
	const std::string_view cr("\n");
	std::string_view data_view(data);
	while (!data_view.empty()) {
		auto cr_pos = data_view.find(cr);
		if (cr_pos == data_view.npos) break;
		std::string_view line_view = data_view.substr(0, cr_pos + cr.size());
		std::string_view all_view = data_view.substr(0, cr_pos);
		auto space = all_view.find("] ");
		if (space == all_view.npos) {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		std::string_view time_view = all_view.substr(1, space - 1);
		all_view.remove_prefix(space + 2);
		space = all_view.find(' ');
		if (space == all_view.npos) {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		std::string_view channel_view = all_view.substr(0, space);
		all_view.remove_prefix(space + 1);
		space = all_view.find(':');
		if (space == all_view.npos) {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		std::string_view name_view = all_view.substr(0, space);
		all_view.remove_prefix(space + 2);
		std::string_view message_view = all_view;
		TimeDetail::TimePoint time;
		if (auto r = TimeDetail::parseGempirTimeString(time_view)) {
			time = *r;
		}
		else {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		lines.emplace_back(
			line_view,
			time_view,
			time,
			name_view,
			message_view
		);

		//advance
		data_view.remove_prefix(cr_pos + cr.size());
	}
	return true;
}

std::string createOverrustleUserTarget(const std::string_view & channel, const std::string_view & user, const date::year_month & ym)
{
	std::stringstream target;
	target
		<< "/"
		<< channel
		<< " chatlog/"
		<< TimeDetail::monthToString(ym.month())
		<< " "
		<< std::to_string(static_cast<int>(ym.year()))
		<< "/userlogs/"
		<< user
		<< ".txt";
	return target.str();
}

std::string createOverrustleChannelTarget(const std::string_view & channel, const date::year_month_day & date)
{
	std::stringstream target;
	target
		<< "/"
		<< channel
		<< " chatlog/"
		<< TimeDetail::monthToString(date.month())
		<< " "
		<< std::to_string(static_cast<int>(date.year()))
		<< "/"
		<< date::format("%F", date)
		<< ".txt";
	return target.str();
}

bool overrustleLogParser(const std::string & data, std::vector<Log::LineView> & lines)
{
	if (data == "didn't find any logs for this user") return false;
	
	const std::string_view cr("\n");

	std::string_view data_view(data);
	while (!data_view.empty()) {
		auto cr_pos = data_view.find(cr);
		if (cr_pos == data_view.npos) break;
		std::string_view line_view = data_view.substr(0, cr_pos + cr.size());
		std::string_view all_view = data_view.substr(0, cr_pos);
		auto space = all_view.find("] ");
		if (space == all_view.npos) {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		std::string_view time_view = all_view.substr(1, space - 1);
		all_view.remove_prefix(space + 2);
		space = all_view.find(' ');
		if (space == all_view.npos) {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		std::string_view name_view = all_view.substr(0, space - 1);
		all_view.remove_prefix(space + 1);
		std::string_view message_view = all_view;
		TimeDetail::TimePoint time;
		if (auto r = TimeDetail::parseOverrustleTimeString(time_view)) {
			time = *r;
		}
		else {
			data_view.remove_prefix(cr_pos + cr.size());
			continue;
		}
		lines.emplace_back(
			line_view,
			time_view,
			time,
			name_view,
			message_view
		);
		//advance
		data_view.remove_prefix(cr_pos + cr.size());
	}
	return true;	
}

