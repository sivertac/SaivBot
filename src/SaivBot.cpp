//SaivBot.cpp
//Author: Sivert Andresen Cubedo

#include "../include/SaivBot.hpp"

SaivBot::SaivBot(boost::asio::io_context & ioc, boost::asio::ssl::context && ctx, const std::filesystem::path & config_path) :
	m_ioc(ioc),
	m_ctx(std::move(ctx)),
	m_stream(ioc, m_ctx),
	m_resolver(ioc),
	m_config_path(config_path),
	m_read_strand(ioc),
	m_send_strand(ioc),
	m_send_message_timer(ioc)
{
	loadConfig(m_config_path);
}

void SaivBot::loadConfig(const std::filesystem::path & path)
{
	if (!std::filesystem::exists(path)) {
		saveConfig(path);
		std::cout << "Edit Config.txt\n";
		throw std::runtime_error("Edit Config.txt");
	}

	nlohmann::json j;
	std::fstream fs(path, std::ios::in);
	if (!fs.is_open()) throw std::runtime_error("Can't open config file");
	fs >> j;
	m_host = j["host"];
	m_port = j["port"];
	m_nick = j["nick"];
	m_password = j["password"];
	nlohmann::from_json(j["modlist"], m_modlist);
	nlohmann::from_json(j["whitelist"], m_whitelist);
	
	for (const std::string & ch : j["channels"]) {
		m_channels.try_emplace(ch, ChannelData(std::make_unique<IRCMessageBuffer>(m_message_buffer_size))); 
	}
}

void SaivBot::saveConfig(const std::filesystem::path & path)
{
	nlohmann::json j;
	j["host"] = m_host;
	j["port"] = m_port;
	j["nick"] = m_nick;
	j["password"] = m_password;
	j["modlist"] = m_modlist;
	j["whitelist"] = m_whitelist;

	{
		std::vector<std::string_view> temp;
		temp.reserve(m_channels.size());
		for (auto & pair : m_channels) {
			temp.emplace_back(pair.first);
		}
		j["channels"] = temp;
	}

	std::fstream fs(path, std::ios::trunc | std::ios::out);
	if (!fs.is_open()) throw std::runtime_error("Can't open config file");
	fs << j;
}

void SaivBot::run()
{
	m_resolver.async_resolve(
		m_host,
		m_port,
		boost::asio::bind_executor(
			m_read_strand,
			std::bind(
				&SaivBot::onResolve,
				this,
				std::placeholders::_1,
				std::placeholders::_2
			)
		)
	);
}

SaivBot::~SaivBot()
{
	saveConfig(m_config_path);
}

void SaivBot::onResolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
{
	if (ec) throw std::runtime_error(ec.message());
	boost::asio::async_connect(
		m_stream.next_layer(),
		results.begin(),
		results.end(),
		std::bind(
			&SaivBot::onConnect,
			this,
			std::placeholders::_1
		)
	);
}

void SaivBot::onConnect(boost::system::error_code ec)
{
	if (ec) throw std::runtime_error(ec.message());
	m_stream.async_handshake(
		ssl::stream_base::client,
		boost::asio::bind_executor(
			m_read_strand,
			std::bind(
				&SaivBot::onHandshake,
				this,
				std::placeholders::_1
			)
		)
	);
}

void SaivBot::onHandshake(boost::system::error_code ec)
{
	if (ec) throw std::runtime_error(ec.message());

	//postSendIRC(std::move(std::string("PASS ").append(m_password)));
	//postSendIRC(std::move(std::string("NICK ").append(m_nick)));

	m_send_queue.emplace(std::move(std::string("PASS ").append(m_password)));
	m_send_queue.emplace(std::move(std::string("NICK ").append(m_nick)));

	m_suspend_read = false;
	m_suspend_send = false;

	std::string channel = formatIRCChannelName(m_nick);
	
	sendJOIN(channel);
	
	sendPRIVMSG(channel, "monkaMEGA");

	for (auto & pair : m_channels) {
		sendJOIN(pair.first);
	}

	//sendWHISPERRequest();

	m_time_started = std::chrono::system_clock::now();

	m_stream.async_read_some(
		boost::asio::buffer(m_recv_buffer),
		boost::asio::bind_executor(
			m_read_strand,
			std::bind(
				&SaivBot::onRead,
				this,
				std::placeholders::_1,
				std::placeholders::_2
			)
		)
	);
}

void SaivBot::onRead(boost::system::error_code ec, std::size_t bytes_transferred)
{
	if (ec) {
		throw std::runtime_error(ec.message());
	}

	m_buffer.append(m_recv_buffer.data(), bytes_transferred);
	parseBuffer();
	consumeMsgBuffer();

	if (!m_suspend_read) {
		m_stream.async_read_some(
			boost::asio::buffer(m_recv_buffer),
			boost::asio::bind_executor(
				m_read_strand,
				std::bind(
					&SaivBot::onRead,
					this,
					std::placeholders::_1,
					std::placeholders::_2
				)
			)
		);
	}
}

void SaivBot::postSendIRC(std::string && msg)
{
	auto handler = [msg = std::move(msg), this]() {
		if (!m_suspend_send) {
			m_send_queue.push(std::move(msg));
			if (!m_send_queue_busy) {
				m_send_queue_busy = true;
				doSendQueue();
			}
		}
	};
	boost::asio::post(
		m_ioc,
		boost::asio::bind_executor(
			m_send_strand,
			handler
		)
	);
}

void SaivBot::doSendQueue()
{
	auto send_handler = [this]() {
		const auto transparent = "\x20\xe2\x81\xad";
		const auto cr = "\r\n";

		std::string & msg = m_send_queue.front();
		if (msg == m_last_message_sendt) {
			msg.append(transparent);
		}
		m_last_message_sendt = msg;
		msg.append(cr);
		boost::asio::async_write(
			m_stream,
			boost::asio::buffer(msg),
			boost::asio::bind_executor(
				m_send_strand,
				std::bind(
					&SaivBot::onSendQueue,
					this,
					std::placeholders::_1,
					std::placeholders::_2
				)
			)
		);
	};
	auto wait_handler = [send_handler, this](boost::beast::error_code ec) {
		if (ec) {
			throw std::runtime_error(ec.message());
		}
		boost::asio::post(
			m_ioc,
			boost::asio::bind_executor(
				m_send_strand,
				send_handler
			)
		);
	};
	auto now = std::chrono::system_clock::now();
	auto next_expire = m_last_message_sendt_time + m_send_message_duration;
	if (now < next_expire) {
		m_send_message_timer.expires_at(next_expire);
		m_send_message_timer.async_wait(
			boost::asio::bind_executor(
				m_send_strand,
				wait_handler
			)
		);
	}
	else {
		send_handler();
	}
}

void SaivBot::onSendQueue(boost::beast::error_code ec, std::size_t bytes_transferred)
{
	if (m_suspend_send) {
		boost::asio::post(
			m_ioc,
			std::bind(
				&SaivBot::reconnectHandler,
				this
			)
		);
		return;
	}
	if (ec) {
		throw std::runtime_error(ec.message());
	}
	m_last_message_sendt_time = std::chrono::system_clock::now();
	m_send_queue.pop();
	if (m_send_queue.empty()) {
		m_send_queue_busy = false;
	}
	else {
		doSendQueue();
	}
}

void SaivBot::parseBuffer()
{
	const std::string cr("\r\n");
	while (true) {
		std::size_t n = m_buffer.find_first_of(cr);
		if (n == m_buffer.npos) return;
		std::string line = m_buffer.substr(0, n);
		m_buffer.erase(m_buffer.begin(), m_buffer.begin() + n + cr.size());
		m_msg_pre_buffer.emplace_back(std::chrono::system_clock::now(), std::move(line));
	}
}


void SaivBot::consumeMsgBuffer()
{
	while (!m_msg_pre_buffer.empty()) {
		auto & irc_msg = m_msg_pre_buffer.front();
		if (irc_msg.getCommand() == "PRIVMSG") {
			parseFreeMessage(irc_msg);
			//commands
			{
				std::string_view local_view = irc_msg.getBody();
				
				if (auto first_word = OptionParser::extractFirstWordDestructive(local_view)) {
					if (caselessCompare(*first_word, m_nick)) {
						auto pair = OptionParser::extractFirstWord(local_view);
						if (!pair.first.empty()) {
							std::string_view second_word = pair.first;
							auto command_it = std::find_if(
								m_command_containers.begin(),
								m_command_containers.end(),
								[&](auto & cc) {return caselessCompare(cc.m_command, second_word); }
							);
							if (command_it != m_command_containers.end()) {
								command_it->m_func(irc_msg, local_view);
							}			
						}
					}
				}
			}
			{
				std::string channel(irc_msg.getParams()[0]);
				auto it = m_channels.find(channel);
				if (it != m_channels.end()) {
					std::get<0>(it->second)->push(std::move(irc_msg));
				}
			}
	
		}
		/*
		else if (irc_msg.getCommand() == "WHISPER") {
			//commands
			{	

				std::string_view local_view = irc_msg.getBody();
				
				if (auto first_word = OptionParser::extractFirstWordDestructive(local_view)) {
					if (caselessCompare(*first_word, m_nick)) {
						auto pair = OptionParser::extractFirstWord(local_view);
						if (!pair.first.empty()) {
							std::string_view second_word = pair.first;
							auto command_it = std::find_if(
								m_command_containers.begin(),
								m_command_containers.end(),
								[&](auto & cc) {return cc.m_command == second_word; }
							);
							if (command_it != m_command_containers.end()) {
								command_it->m_func(irc_msg, local_view);
							}			
						}
					}	
				}
			}
		}
		*/
		else {
			if (irc_msg.getCommand() == "PING") {
				postSendIRC("PONG");
			}
			else if (irc_msg.getCommand() == "RECONNECT") {
				std::cout << "Reconnecting\n";
				postDoRECONNECT();
			}
			else if (caselessCompare(irc_msg.getNick(), m_nick)) {
				if (irc_msg.getCommand() == "JOIN") {
					std::string channel(irc_msg.getParams()[0]);
					auto it = m_channels.find(channel);
					if (it == m_channels.end()) {
						m_channels.emplace(
							channel,
							ChannelData(
								std::make_unique<IRCMessageBuffer>(m_message_buffer_size)
							)
						);
						saveConfig(m_config_path);
					}	
				}
				else if (irc_msg.getCommand() == "PART") {
					std::string channel(irc_msg.getParams()[0]);
					auto it = m_channels.find(channel);
					if (it != m_channels.end()) {
						m_channels.erase(it);
						saveConfig(m_config_path);
					}
				}
			}
			std::cout << irc_msg.getData() << "\n";
		}
		m_msg_pre_buffer.pop_front(); //POP!!!
	}
}

void SaivBot::parseFreeMessage(const IRCMessage & msg)
{
	if (msg.getBody().find("A multi-raffle has begun") != msg.getBody().npos) {
		sendPRIVMSG(msg.getParams()[0], "!join");
	}	
}

void SaivBot::sendPRIVMSG(std::string_view channel, std::string_view msg)
{
#ifdef SaivBot_TESTMODE
	postSendIRC(std::move(std::string("PRIVMSG ").append(channel).append(" :").append(msg).append(" TEST")));
#else
	postSendIRC(std::move(std::string("PRIVMSG ").append(channel).append(" :").append(msg)));
#endif
}

void SaivBot::sendJOIN(std::string_view channel)
{
	postSendIRC(std::move(std::string("JOIN ").append(channel)));
}

void SaivBot::sendPART(std::string_view channel)
{
	postSendIRC(std::move(std::string("PART ").append(channel)));
}

void SaivBot::sendWHISPERRequest()
{
	postSendIRC(std::move(std::string("CAP REQ :twitch.tv/commands")));	
}

void SaivBot::replyToIRCMessage(const IRCMessage & msg, std::string_view reply)
{
	if (msg.getCommand() == "PRIVMSG") {
		sendPRIVMSG(msg.getParams()[0], reply);
	}
	else if (msg.getCommand() == "WHISPER") {
		sendWHISPER(msg.getNick(), reply);
	}
	else {
		throw std::runtime_error("Can't reply to IRCMessage");
	}
}

void SaivBot::doShutdown()
{
	auto stream_handler = [this](boost::system::error_code ec) {
		if (ec) {
			throw std::runtime_error(ec.message());
		}
		m_stream.next_layer().close();
	};
	m_stream.async_shutdown(
		stream_handler
	);
}

void SaivBot::postDoRECONNECT()
{
	auto suspend_write_handler = [this]() {
		std::cout << "suspend_write_handler\n";
		m_suspend_send = true;
		if (!m_send_queue_busy) {
			boost::asio::post(
				m_ioc,
				std::bind(
					&SaivBot::reconnectHandler,
					this
				)
			);
		}
		//else wait for send queue handler to post reconnectHandler
	};

	m_suspend_read = true;

	boost::asio::post(
		m_ioc,
		boost::asio::bind_executor(
			m_send_strand,
			suspend_write_handler
		)
	);
}

void SaivBot::reconnectHandler()
{
	m_send_queue = std::queue<std::string>();
	m_send_queue_busy = false;
	run();
}

void SaivBot::sendWHISPER(std::string_view target, std::string_view msg)
{
	std::stringstream ss;
	ss 
		<< "PRIVMSG #jtv :"
		<< "/w "
		<< target
		<< " "
		<< msg;

	postSendIRC(std::move(ss.str()));
	std::cout << ss.str() << "\n";
}

bool SaivBot::isModerator(std::string_view user)
{
	std::string str;
	std::transform(user.begin(), user.end(), std::back_inserter(str), ::tolower);
	return m_modlist.find(str) != m_modlist.end();
}

bool SaivBot::isWhitelisted(std::string_view user)
{
	std::string str;
	std::transform(user.begin(), user.end(), std::back_inserter(str), ::tolower);
	return m_whitelist.find(str) != m_whitelist.end();
}

void SaivBot::shutdownCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {
		doShutdown();
	}
}

void SaivBot::helpCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {

		using namespace OptionParser;

		Parser parser(Option<WordType>(m_command_containers[Commands::help_command].m_command));
		
		auto set = parser.parse(input_line);

		if (auto r = set.find<0>()) {

			auto command = r->get<0>();

			auto it = std::find_if(m_command_containers.begin(), m_command_containers.end(), [&](auto & con) {return caselessCompare(con.m_command, command); });
			
			if (it != m_command_containers.end()) {
				std::stringstream reply;
				reply << msg.getNick() << ", " << it->m_description << " Usage: " << it->m_command << " " << it->m_arguments;
				sendPRIVMSG(msg.getParams()[0], reply.str());
			}
		}
	}
}

void SaivBot::countCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {
		using namespace OptionParser;
		Parser parser(
			Option<StringType>(m_command_containers[Commands::count_command].m_command),
			Option<WordType>(m_command_containers[Commands::count_command].m_command),
			Option<ListType>("-channel"),
			Option<WordType>("-channel"),
			Option<ListType>("-user"),
			Option<WordType>("-user"),
			Option<>("-allusers"),
			Option<WordType, WordType>("-period"),
			Option<>("-caseless"),
			Option<WordType>("-service"),
			Option<>("-regex")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		if (auto result = set.find<0>()) { //target string
			search_str = result->get<0>();
		}
		else if (auto result = set.find<1>()) { //target word
			search_str = result->get<0>();
		}
		else {
			return;
		}

		std::vector<std::string_view> channels;
		if (auto result = set.find<2>()) { //channel list
			channels = result->get<0>();
		}
		else if (auto result = set.find<3>()) {//channel word
			channels = decltype(channels){result->get<0>()};
		}
		else {
			channels = decltype(channels){msg.getParams()[0]};
			if (channels.front()[0] == '#') {
				channels.front().remove_prefix(1);
			}
		}

		std::vector<std::string_view> users;
		bool all_users = false;
		if (auto result = set.find<4>()) { //user list
			users = result->get<0>();
		}
		else if (auto result = set.find<5>()) { //user word
			users = decltype(users){result->get<0>()};
		}
		else if (set.find<6>()) {
			all_users = true;
		}
		else {
			users = decltype(users){msg.getNick()};
		}
		
		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));
		TimeDetail::TimePeriod period(
			TimeDetail::TimePoint(date::sys_days(current_date.year() / current_date.month() / 1)),
			TimeDetail::TimePoint(date::sys_days((current_date.year() / current_date.month() / 1) + date::months(1)))
		);
		if (auto result = set.find<7>()) { //period
			if (auto r2 = TimeDetail::parseTimePeriod(result->get<0>(), result->get<1>())) {
				period = *r2;
			}
			else {
				replyToIRCMessage(msg, std::string(msg.getNick()).append(", invalid period NaM"));
				return;
			}
		}

		std::function<bool(char, char)> predicate;
		if (set.find<8>()) { //predecate (-caseless)
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		LogService service;
		if (auto r = set.find<9>()) { //service
			auto s = r->get<0>();
			if (caselessCompare(s, "gempir")) {
				service = LogService::gempir_log;
			}
			else if (caselessCompare(s, "overrustle")) {
				service = LogService::overrustle_log;
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid service NaM"));
				return;
			}
		}
		else {
			service = LogService::gempir_log;
		}

		bool regex = false;
		if (auto r = set.find<10>()) {
			regex = true;
		}
		
		//set up log request
		LogRequest log_request;
		{
			fillLogRequestTargetFields(log_request, service, all_users, period, channels, users);
			auto shared_data_ptr = std::make_shared<CountCallbackSharedData>();
			shared_data_ptr->reference_count = log_request.targets.size();
			if (!regex) {
				shared_data_ptr->count_func = [search_str = std::move(search_str), predicate](std::string_view str) -> std::size_t {
					auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
					return countTargetOccurrences(str.begin(), str.end(), searcher);
				};
			}
			else {
				auto regex_searcher = std::regex(search_str);
				shared_data_ptr->count_func = [regex_searcher = std::move(regex_searcher)](std::string_view str) -> std::size_t {
					return countTargetOccurrences(str, regex_searcher);
				};
			}
			shared_data_ptr->period = period;
			shared_data_ptr->irc_msg = msg;
			shared_data_ptr->shared_count = 0;

			log_request.callback = std::bind(
				&SaivBot::countCommandCallback,
				this,
				std::placeholders::_1,
				shared_data_ptr
			);

			log_request.error_handler = std::bind(
				&SaivBot::countCommandErrorHandler,
				this,
				shared_data_ptr
			);

		}
		std::make_shared<LogDownloader>(m_ioc)->run(std::move(log_request));
	}
}

void SaivBot::findCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {
		using namespace OptionParser;
		Parser parser(
			Option<StringType>(m_command_containers[Commands::find_command].m_command),
			Option<WordType>(m_command_containers[Commands::find_command].m_command),
			Option<ListType>("-channel"),
			Option<WordType>("-channel"),
			Option<ListType>("-user"),
			Option<WordType>("-user"),
			Option<>("-allusers"),
			Option<WordType, WordType>("-period"),
			Option<>("-caseless"),
			Option<WordType>("-service"),
			Option<>("-regex")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		if (auto result = set.find<0>()) { //target string
			search_str = result->get<0>();
		}
		else if (auto result = set.find<1>()) { //target word
			search_str = result->get<0>();
		}
		else {
			return;
		}

		std::vector<std::string_view> channels;
		if (auto result = set.find<2>()) { //channel list
			channels = result->get<0>();
		}
		else if (auto result = set.find<3>()) {//channel word
			channels = decltype(channels){result->get<0>()};
		}
		else {
			channels = decltype(channels){msg.getParams()[0]};
			if (channels.front()[0] == '#') {
				channels.front().remove_prefix(1);
			}
		}

		std::vector<std::string_view> users;
		bool all_users = false;
		if (auto result = set.find<4>()) { //user list
			users = result->get<0>();
		}
		else if (auto result = set.find<5>()) { //user word
			users = decltype(users){result->get<0>()};
		}
		else if (set.find<6>()) {
			all_users = true;
		}
		else {
			users = decltype(users){msg.getNick()};
		}

		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));
		TimeDetail::TimePeriod period(
			TimeDetail::TimePoint(date::sys_days(current_date.year() / current_date.month() / 1)),
			TimeDetail::TimePoint(date::sys_days((current_date.year() / current_date.month() / 1) + date::months(1)))
		);
		if (auto result = set.find<7>()) { //period
			if (auto r2 = TimeDetail::parseTimePeriod(result->get<0>(), result->get<1>())) {
				period = *r2;
			}
			else {
				replyToIRCMessage(msg, std::string(msg.getNick()).append(", invalid period NaM"));
				return;
			}
		}

		std::function<bool(char, char)> predicate;
		if (set.find<8>()) { //predecate (-caseless)
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		LogService service;
		if (auto r = set.find<9>()) { //service
			auto s = r->get<0>();
			if (caselessCompare(s, "gempir")) {
				service = LogService::gempir_log;
			}
			else if (caselessCompare(s, "overrustle")) {
				service = LogService::overrustle_log;
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid service NaM"));
				return;
			}
		}
		else {
			service = LogService::gempir_log;
		}

		bool regex = false;
		if (auto r = set.find<10>()) {
			regex = true;
		}

		//set up log request
		LogRequest log_request;
		{
			fillLogRequestTargetFields(log_request, service, all_users, period, channels, users);
			auto shared_data_ptr = std::make_shared<FindCallbackSharedData>();
			shared_data_ptr->reference_count = log_request.targets.size();
			if (!regex) {
				shared_data_ptr->find_func = [search_str = std::move(search_str), predicate](std::string_view str)->std::size_t {
					auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
					return std::search(str.begin(), str.end(), searcher) != str.end();
				};
			}
			else {
				auto regex_searcher = std::regex(search_str);	
				shared_data_ptr->find_func = [regex_searcher = std::move(regex_searcher)](std::string_view str)->std::size_t {
					return std::regex_match(str.begin(), str.end(), regex_searcher);
				};
			}
			shared_data_ptr->dump_func = [](std::vector<Log::Line> & lines) {
				std::sort(
					lines.begin(),
					lines.end(),
					[](auto & a, auto & b) {return a.getTime() < b.getTime(); }
				);
				std::stringstream ss;
				for (auto & line : lines) {
					using namespace date;
					ss << line.getTime() << " " << line.getNameView() << ": " << line.getMessageView() << "\n";
				}
				return ss.str();
			};

			shared_data_ptr->period = period;
			shared_data_ptr->irc_msg = msg;
			
			log_request.callback = std::bind(
				&SaivBot::findCommandCallback,
				this,
				std::placeholders::_1,
				shared_data_ptr
			);

			log_request.error_handler = std::bind(
				&SaivBot::findCommandErrorHandler,
				this,
				shared_data_ptr
			);
		}
		std::make_shared<LogDownloader>(m_ioc)->run(std::move(log_request));
	}
}

void SaivBot::clipCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {
		using namespace OptionParser;
		OptionParser::Parser parser(Option<NumberType<std::size_t>>("-lines_from_now"));
		auto set = parser.parse(input_line);

		std::string channel(msg.getParams()[0]);
		auto it = m_channels.find(channel);
		if (it != m_channels.end()) {
			IRCMessageBuffer & msg_buffer = *std::get<0>(it->second);

			if (auto r = set.find<0>()) {
				std::size_t line_count = r->get<0>();
				if (line_count > 0) {
					msg_buffer.accessLinesFromNow(
						line_count,
						[&](const IRCMessageBuffer::LinesFromNowVecType & lines)
					{
						std::stringstream ss;
						for (const IRCMessage & irc_msg : lines) {
							using namespace date;
							ss << irc_msg.getTime() << " " << irc_msg.getNick() << ": " << irc_msg.getBody() << "\n";
						}
						std::make_shared<DankHttp::NuulsUploader>(m_ioc)->run(
							std::bind(
								&SaivBot::clipCommandCallback,
								this,
								std::placeholders::_1,
								std::make_shared<IRCMessage>(msg)
							),
							ss.str(),
							"i.nuuls.com",
							"443",
							"/upload"
						);
					}
					);
				}
			}
			else {
				std::stringstream reply;
				reply << msg.getNick();
				reply << ", " << "Invalid params NaM";
				sendPRIVMSG(msg.getParams()[0], reply.str());
			}
		}
	}
}

void SaivBot::promoteCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {

		using namespace OptionParser;

		Parser parser(Option<WordType>(m_command_containers[Commands::promote_command].m_command));
		auto set = parser.parse(input_line);
		if (auto r = set.find<0>()) {
			auto ret_str = r->get<0>();
			std::string user;
			std::transform(ret_str.begin(), ret_str.end(), std::back_inserter(user), ::tolower);
			if (m_whitelist.find(user) == m_whitelist.end()) {
				m_whitelist.emplace(user);
				sendPRIVMSG(msg.getParams()[0], user + " promoted");
				saveConfig(m_config_path);
			}
		}
	}
}

void SaivBot::demoteCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {

		using namespace OptionParser;

		Parser parser(Option<WordType>(m_command_containers[Commands::demote_command].m_command));
		auto set = parser.parse(input_line);
		if (auto r = set.find<0>()) {
			auto ret_str = r->get<0>();
			std::string user;
			std::transform(ret_str.begin(), ret_str.end(), std::back_inserter(user), ::tolower);
			auto it = m_whitelist.find(user);
			if (it != m_whitelist.end()) {
				m_whitelist.erase(it);
				sendPRIVMSG(msg.getParams()[0], user + " demoted");
				saveConfig(m_config_path);
			}
		}
	}
}

void SaivBot::joinCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {

		using namespace OptionParser;

		Parser parser(Option<WordType>(m_command_containers[Commands::join_command].m_command));
		auto set = parser.parse(input_line);
		if (auto r = set.find<0>()) {
			std::string channel("#");
			channel.append(r->get<0>());
			std::transform(channel.begin(), channel.end(), channel.begin(), ::tolower);
			sendJOIN(channel);
			sendPRIVMSG(channel, "monkaMEGA");
		}
	}
}

void SaivBot::partCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {
		using namespace OptionParser;	
		Parser parser(Option<WordType>(m_command_containers[Commands::part_command].m_command));
		auto set = parser.parse(input_line);
		if (auto r = set.find<0>()) {
			std::string channel = formatIRCChannelName(r->get<0>());
			sendPART(channel);
		}
	}
}

void SaivBot::uptimeCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {
		std::chrono::system_clock::duration d = std::chrono::system_clock::now() - m_time_started;
		std::stringstream s;
		using namespace date;
		s << msg.getNick() << ", " << std::chrono::floor<std::chrono::seconds>(d);
		sendPRIVMSG(msg.getParams()[0], s.str());
	}
}

void SaivBot::sayCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {
		using namespace OptionParser;	
		Parser parser(
			Option<StringType>(m_command_containers[Commands::say_command].m_command),
			Option<WordType>("-channel"),
			Option<NumberType<std::size_t>>("-echo")
		);
		auto set = parser.parse(input_line);
		if (auto r = set.find<0>()) {
			auto str = r->get<0>();
			std::string_view channel = msg.getParams()[0];
			if (auto r1 = set.find<1>()) {
				channel = formatIRCChannelName(r1->get<0>());
			}
			std::size_t echo = 1;
			if (auto r1 = set.find<2>()) {
				echo = r1->get<0>();
				if (echo > 5) echo = 5;
			}
			
			for (std::size_t i = 0; i < echo; ++i) {
				sendPRIVMSG(channel, str);
			}
		}
		else {
			if (auto first_word = OptionParser::extractFirstWordDestructive(input_line)) {
				sendPRIVMSG(msg.getParams()[0], input_line);
			}
		}
	}
}

void SaivBot::pingCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	std::stringstream ss;
	ss << msg.getNick() << ", " << "PONG NaM \xE2\x80\xBC";	
	replyToIRCMessage(msg, ss.str());
}

void SaivBot::commandsCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	const auto commands_url = "https://github.com/SaivNator/SaivBot#commands";
	std::stringstream ss;
	ss << msg.getNick() << ", " << commands_url;
	replyToIRCMessage(msg, ss.str());
}

void SaivBot::flagsCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	const auto flags_url = "https://github.com/SaivNator/SaivBot#flags";
	std::stringstream ss;
	ss << msg.getNick() << ", " << flags_url;
	replyToIRCMessage(msg, ss.str());
}

void SaivBot::test_insertmessageCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getUser())) {
		using namespace OptionParser;
		Parser parser(Option<StringType>(m_command_containers[Commands::test_insertmessage_command].m_command));
		auto set = parser.parse(input_line);
		if (auto result = set.find<0>()) {
			auto insert_msg_handler = [str = std::string(result->get<0>()), this]() {
				m_msg_pre_buffer.emplace_back(std::chrono::system_clock::now(), std::move(std::string(str)));
				consumeMsgBuffer();
			};

			boost::asio::post(
				m_ioc,
				boost::asio::bind_executor(
					m_read_strand,
					insert_msg_handler
				)
			);
		}
	}
}

void SaivBot::clipCommandCallback(std::string && str, ClipCallbackSharedPtr ptr)
{
	nuulsServerReply(str, *ptr);
}
void SaivBot::nuulsServerReply(const std::string & str, const IRCMessage & msg)
{
	std::stringstream reply;
	reply << msg.getNick() << ", " << str;
	replyToIRCMessage(msg, reply.str());
}

std::vector<date::year_month> SaivBot::periodToYearMonths(const TimeDetail::TimePeriod & period)
{
	std::vector<date::year_month> year_months;	
	date::year_month_day ymd = date::floor<date::days>(period.begin());
	date::year_month begin_ym(ymd.year(), ymd.month());
	ymd = date::floor<date::days>(period.end());
	date::year_month end_ym(ymd.year(), ymd.month());
	end_ym += date::months(1);
	while (begin_ym < end_ym) {
		year_months.push_back(begin_ym);
		begin_ym += date::months(1);
	}
	return year_months;
}

std::vector<date::year_month_day> SaivBot::periodToDates(const TimeDetail::TimePeriod & period)
{
	std::vector<date::year_month_day> dates;
	date::year_month_day begin = date::floor<date::days>(period.begin());
	date::year_month_day end = date::floor<date::days>(period.end());
	end = date::sys_days{ end } + date::days(1);
	while (begin < end) {
		dates.push_back(begin);
		begin = date::sys_days{ begin } + date::days(1);
	}
	return dates;
}

std::size_t countTargetOccurrences(std::string_view str, const std::regex & regex)
{
	std::match_results<std::string_view::const_iterator> m;
	std::size_t count = 0;
	auto begin = str.cbegin();
	auto end = str.cend();
	while (std::regex_search(begin, end, m, regex)) {
		++count;
		begin = m.suffix().first;
	}

	return count;
}

bool caselessCompare(std::string_view str1, std::string_view str2)
{
	if (str1.size() != str2.size()) {
		return false;
	}
	for (std::size_t i = 0; i < str1.size(); ++i) {
		if (std::tolower(str1[i]) != std::tolower(str2[i])) {
			return false;
		}
	}
	return true;
}
