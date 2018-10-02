//SaivBot.cpp
//Author: Sivert Andresen Cubedo

#include "../include/SaivBot.hpp"

SaivBot::SaivBot(boost::asio::io_context & ioc, const std::filesystem::path & config_path) :
	m_ioc(ioc),
	m_resolver(ioc),
	m_sock(ioc),
	m_config_path(config_path)
{
	loadConfig(m_config_path);
}

void SaivBot::loadConfig(const std::filesystem::path & path)
{
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
	std::fstream fs(path, std::ios::trunc | std::ios::out);
	if (!fs.is_open()) throw std::runtime_error("Can't open config file");
	fs << j;
}

void SaivBot::run()
{
	m_running = true;

	m_resolver.async_resolve(
		m_host,
		m_port,
		std::bind(
			&SaivBot::resolveHandler,
			this,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
}

SaivBot::~SaivBot()
{
	saveConfig(m_config_path);
}

void SaivBot::resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results)
{
	boost::asio::async_connect(
		m_sock,
		results.begin(),
		results.end(),
		std::bind(
			&SaivBot::connectHandler,
			this,
			std::placeholders::_1
		)
	);
	if (ec) throw std::runtime_error(ec.message());
}

void SaivBot::connectHandler(boost::system::error_code ec)
{
	sendIRC("PASS " + m_password);

	sendIRC("NICK " + m_nick);

	std::string channel("#"); 
	channel.append(m_nick);
	std::transform(channel.begin(), channel.end(), channel.begin(), ::tolower);

	sendJOIN(channel);
	sendPRIVMSG(channel, "monkaMEGA");

	m_sock.async_read_some(
		boost::asio::buffer(m_recv_buffer),
		std::bind(
			&SaivBot::receiveHandler,
			this,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
	if (ec) throw std::runtime_error(ec.message());
}

void SaivBot::receiveHandler(boost::system::error_code ec, std::size_t ret)
{
	m_buffer.append(m_recv_buffer.data(), ret);
	
	parseBuffer();
	consumeMsgBuffer();

	if (!m_running) return;
	if (!m_sock.is_open()) throw std::runtime_error("Unexpected closed socket");

	m_sock.async_read_some(
		boost::asio::buffer(m_recv_buffer),
		std::bind(
			&SaivBot::receiveHandler,
			this,
			std::placeholders::_1,
			std::placeholders::_2
		)
	);
	if (ec) throw std::runtime_error(ec.message());
}

void SaivBot::sendIRC(const std::string & msg)
{
	const std::string cr("\r\n");
	
	{
		std::lock_guard<std::mutex> lock(m_send_mutex);
		
		std::shared_ptr<std::string> msg_ptr = std::make_shared<std::string>(msg);
		
		if (*msg_ptr == m_last_message_queued) {
			msg_ptr->append("\x20\xe2\x81\xad");
		}

		m_last_message_queued = *msg_ptr;

		msg_ptr->append(cr);

		auto now = std::chrono::system_clock::now();

		if (m_next_message_time < now) {
			m_next_message_time = now + std::chrono::milliseconds(1750);
			boost::asio::async_write(
				m_sock,
				boost::asio::buffer(*msg_ptr),
				std::bind(
					&SaivBot::sendHandler,
					this,
					std::placeholders::_1,
					std::placeholders::_2,
					msg_ptr
				)
			);
		}
		else {
			auto timer_ptr = std::make_shared<boost::asio::system_timer>(m_ioc);
			timer_ptr->expires_at(m_next_message_time);

			m_next_message_time += std::chrono::milliseconds(1750);

			timer_ptr->async_wait(
				std::bind(
					&SaivBot::sendTimerHandler,
					this,
					std::placeholders::_1,
					timer_ptr,
					msg_ptr
				)
			);
		}
	}
}

void SaivBot::sendTimerHandler(boost::system::error_code ec, std::shared_ptr<boost::asio::system_timer> timer_ptr, std::shared_ptr<std::string> ptr)
{

	boost::asio::async_write(
		m_sock,
		boost::asio::buffer(*ptr),
		std::bind(
			&SaivBot::sendHandler,
			this,
			std::placeholders::_1,
			std::placeholders::_2,
			ptr
		)
	);
	if (ec) throw std::runtime_error(ec.message());
}

void SaivBot::sendHandler(boost::system::error_code ec, std::size_t ret, std::shared_ptr<std::string> ptr)
{
	if (ec) throw std::runtime_error(ec.message());
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
		if (irc_msg.getCommand() != "PRIVMSG") {
			if (irc_msg.getCommand() == "PING") {
				sendIRC("PONG");
			}
			else if (caselessCompare(irc_msg.getNick(), m_nick)) {
				if (irc_msg.getCommand() == "JOIN") {
					std::string channel(irc_msg.getParams()[0]);
					if (m_channels.find(channel) == m_channels.end()) {
						ChannelData data = std::make_tuple(std::make_unique<IRCMessageBuffer>(m_message_buffer_size));
						m_channels.emplace(std::move(channel), std::move(data));
					}
				}
				else if (irc_msg.getCommand() == "PART") {
					std::string channel(irc_msg.getParams()[0]);
					auto it = m_channels.find(channel);
					if (it != m_channels.end()) {
						m_channels.erase(it);
					}
				}
			}
			std::cout << irc_msg.getData() << "\n";
		}
		else {
			//xD
			if (isInPrefixCaseless(irc_msg.getBody(), std::string_view("!xd"))) {
				sendPRIVMSG(irc_msg.getParams()[0], "xD");
			}
			else if (isInPrefixCaseless(irc_msg.getBody(), std::string_view("!NaM"))) {
				sendPRIVMSG(irc_msg.getParams()[0], "NaM");
			}
			else if (irc_msg.getBody().find("A multi-raffle has begun") != irc_msg.getBody().npos) {
				sendPRIVMSG(irc_msg.getParams()[0], "!join");
			}
			
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
			{
				std::string channel(irc_msg.getParams()[0]);
				auto it = m_channels.find(channel);
				if (it != m_channels.end()) {
					std::get<0>(it->second)->push(std::move(irc_msg));
				}
			}
		}
		m_msg_pre_buffer.pop_front();
	}
}

void SaivBot::sendPRIVMSG(const std::string_view & channel, const std::string_view & msg)
{
	sendIRC(std::string("PRIVMSG ").append(channel).append(" :").append(msg));
}

void SaivBot::sendJOIN(const std::string_view & channel)
{
	sendIRC(std::string("JOIN ").append(channel));
}

void SaivBot::sendPART(const std::string_view & channel)
{
	sendIRC(std::string("PART ").append(channel));
}

bool SaivBot::isModerator(const std::string_view & user)
{
	std::string str;
	std::transform(user.begin(), user.end(), std::back_inserter(str), ::tolower);
	return m_modlist.find(str) != m_modlist.end();
}

bool SaivBot::isWhitelisted(const std::string_view & user)
{
	std::string str;
	std::transform(user.begin(), user.end(), std::back_inserter(str), ::tolower);
	return m_whitelist.find(str) != m_whitelist.end();
}

void SaivBot::shutdownCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isModerator(msg.getNick())) {
		m_running = false;
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
			Option<WordType>("-channel"),
			Option<WordType>("-user"),
			Option<WordType, WordType>("-period"),
			Option<>("-caseless")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		std::string channel;
		std::string user;

		std::function<bool(char, char)> predicate;

		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));

		TimeDetail::TimePeriod period(
			TimeDetail::TimePoint(date::sys_days(current_date.year() / current_date.month() / 1)),
			TimeDetail::TimePoint(date::sys_days((current_date.year() / current_date.month() / 1) + date::months(1)))
		);

		if (auto r = set.find<0>()) { //target string
			search_str = r->get<0>();
		}
		else if (auto r = set.find<1>()) { //target word
			search_str = r->get<0>();
		}
		else {
			return;
		}

		if (auto r = set.find<2>()) { //channel
			channel = r->get<0>();
		}
		else {
			channel = msg.getParams()[0];
		}

		if (auto r = set.find<3>()) { //user
			user = r->get<0>();
		}
		else {
			user = msg.getNick();
		}

		if (auto r = set.find<4>()) { //period
			if (auto r2 = TimeDetail::parseTimePeriod(r->get<0>(), r->get<1>())) {
				period = *r2;
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid period NaM"));
				return;
			}
		}

		if (auto r = set.find<5>()) {
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		//gather months and years
		std::vector<date::year_month> year_months = periodToYearMonths(period);
		if (year_months.empty()) {
			sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid period NaM"));
			return;
		}

		if (channel[0] == '#') channel.erase(0, 1);
		
		const std::string host("api.gempir.com");
		const std::string port("443");

		CountCallbackSharedPtr callback_ptr = std::make_shared<CountCallbackSharedPtr::element_type>();
		std::get<1>(*callback_ptr) = year_months.size();
		std::get<2>(*callback_ptr) = std::move(search_str);
		std::get<3>(*callback_ptr) = predicate;
		std::get<4>(*callback_ptr) = msg;
		std::get<5>(*callback_ptr) = period;
		std::get<6>(*callback_ptr) = 0;
		
		for (auto & ym : year_months) {
			std::make_shared<GempirUserLogDownloader>(m_ioc)->run(
				std::bind(
					&SaivBot::countCommandCallback,
					this,
					std::placeholders::_1,
					callback_ptr
				),
				host,
				port,
				channel,
				user,
				ym.month(),
				ym.year()
			);
		}
	}
}

void SaivBot::findCommandFunc(const IRCMessage & msg, std::string_view input_line)
{
	if (isWhitelisted(msg.getNick())) {

		using namespace OptionParser;

		Parser parser(
			Option<StringType>(m_command_containers[Commands::find_command].m_command),
			Option<WordType>(m_command_containers[Commands::find_command].m_command),
			Option<WordType>("-channel"),
			Option<WordType>("-user"),
			Option<WordType, WordType>("-period"),
			Option<>("-caseless")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		std::string channel;
		std::string user;

		std::function<bool(char, char)> predicate;

		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));

		TimeDetail::TimePeriod period(
			TimeDetail::TimePoint(date::sys_days(current_date.year() / current_date.month() / 1)),
			TimeDetail::TimePoint(date::sys_days((current_date.year() / current_date.month() / 1) + date::months(1)))
		);

		if (auto r = set.find<0>()) { //target string
			search_str = r->get<0>();
		}
		else if (auto r = set.find<1>()) { //target word
			search_str = r->get<0>();
		}
		else {
			return;
		}

		if (auto r = set.find<2>()) { //channel
			channel = r->get<0>();
		}
		else {
			channel = msg.getParams()[0];
		}

		if (auto r = set.find<3>()) { //user
			user = r->get<0>();
		}
		else {
			user = msg.getNick();
		}

		if (auto r = set.find<4>()) { //period
			if (auto r2 = TimeDetail::parseTimePeriod(r->get<0>(), r->get<1>())) {
				period = *r2;
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid period NaM"));
				return;
			}
		}

		if (auto r = set.find<5>()) {
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		//gather months and years
		std::vector<date::year_month> year_months = periodToYearMonths(period);
		if (year_months.empty()) {
			sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid period NaM"));
			return;
		}

		if (channel[0] == '#') channel.erase(0, 1);

		const std::string host("api.gempir.com");
		const std::string port("443");

		FindCallbackSharedPtr callback_ptr = std::make_shared<FindCallbackSharedPtr::element_type>();
		std::get<1>(*callback_ptr) = year_months.size();
		std::get<2>(*callback_ptr) = std::move(search_str);
		std::get<3>(*callback_ptr) = predicate;
		std::get<4>(*callback_ptr) = msg;
		std::get<5>(*callback_ptr) = period;

		for (auto & ym : year_months) {
			std::make_shared<GempirUserLogDownloader>(m_ioc)->run(
				std::bind(
					&SaivBot::findCommandCallback,
					this,
					std::placeholders::_1,
					callback_ptr
				),
				host,
				port,
				channel,
				user,
				ym.month(),
				ym.year()
			);
		}
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
			std::string channel("#");
			channel.append(r->get<0>());
			std::transform(channel.begin(), channel.end(), channel.begin(), ::tolower);
			sendPART(channel);
		}
	}
}

//void SaivBot::countCommandCallback(Log && log, std::shared_ptr<std::pair<std::mutex, std::size_t>> mutex_ptr, std::shared_ptr<const std::string> search_ptr, std::shared_ptr<const IRCMessage> msg_ptr, std::shared_ptr<std::vector<Log>> log_container_ptr)
void SaivBot::countCommandCallback(Log && log, CountCallbackSharedPtr ptr)
{
	std::mutex & mutex = std::get<0>(*ptr);
	std::size_t & mutex_count = std::get<1>(*ptr);
	std::string & search_str = std::get<2>(*ptr);
	auto predicate = std::get<3>(*ptr);
	const IRCMessage & irc_msg = std::get<4>(*ptr);
	const TimeDetail::TimePeriod & period = std::get<5>(*ptr);
	std::size_t & current_count = std::get<6>(*ptr);

	//count log
	auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
	//auto searcher = std::boyer_moore_searcher(search_str.begin(), search_str.end(), std::hash<char>(), predicate);
	//auto searcher = std::boyer_moore_horspool_searcher(search_str.begin(), search_str.end(), std::hash<char>(), predicate);
	std::size_t count = 0;
	if (log.isValid()) {
		for (auto & line : log.getLines()) {
			if (period.isInside(line.getTime())) {
				auto & msg = line.getMessageView();
				count += countTargetOccurrences(msg.begin(), msg.end(), searcher);
			}
		}
	}

	{
		std::lock_guard<std::mutex> lock(mutex);
		--mutex_count;

		current_count += count;

		if (mutex_count == 0) {			
			std::stringstream reply;
			reply << irc_msg.getNick() << ", count: " << current_count;
			sendPRIVMSG(irc_msg.getParams()[0], reply.str());
		}
	}
}

void SaivBot::findCommandCallback(Log && log, FindCallbackSharedPtr ptr)
{
	std::mutex & mutex = std::get<0>(*ptr);
	std::size_t & mutex_count = std::get<1>(*ptr);
	std::string & search_str = std::get<2>(*ptr);
	auto predicate = std::get<3>(*ptr);
	const IRCMessage & irc_msg = std::get<4>(*ptr);
	const TimeDetail::TimePeriod & period = std::get<5>(*ptr);
	std::vector<Log> & log_container = std::get<6>(*ptr);
	std::vector<std::vector<std::reference_wrapper<const Log::LineView>>> & lineviewref_vec = std::get<7>(*ptr);

	//find relevant lines
	auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
	std::vector<std::reference_wrapper<const Log::LineView>> lines_found;
	if (log.isValid()) {
		for (auto & line : log.getLines()) {
			auto & time = line.getTime();
			if (period.isInside(time)) {
				auto & msg = line.getMessageView();
				if (std::search(msg.begin(), msg.end(), searcher) != msg.end()) {
					lines_found.push_back(line);
				}
			}
		}
	}


	{
		std::lock_guard<std::mutex> lock(mutex);
		--mutex_count;
		
		if (!lines_found.empty()) {
			log_container.push_back(std::move(log));
			lineviewref_vec.push_back(std::move(lines_found));
		}

		if (mutex_count == 0) {
			if (!lineviewref_vec.empty()) {
				std::vector<std::reference_wrapper<const Log::LineView>> all_lines;
				for (auto & v : lineviewref_vec) {
					all_lines.insert(all_lines.end(), v.begin(), v.end());
				}
				std::sort(all_lines.begin(), all_lines.end(), [](auto & a, auto & b) {return a.get().getTime() < b.get().getTime(); });

				std::stringstream lines_found_stream;

				for (auto & line : all_lines) {
					using namespace date;
					lines_found_stream << line.get().getTime() << ": " << line.get().getMessageView() << "\n";
				}

				std::make_shared<DankHttp::NuulsUploader>(m_ioc)->run(
					std::bind(
						&SaivBot::findCommandCallback2,
						this,
						std::placeholders::_1,
						ptr
					),
					lines_found_stream.str(),
					"i.nuuls.com",
					"443",
					"/upload"
				);
			}
			else {
				std::stringstream reply;
				reply << irc_msg.getNick() << ", no hit NaM";
				sendPRIVMSG(irc_msg.getParams()[0], reply.str());
			}
		}
	}
}

void SaivBot::findCommandCallback2(std::string && str, FindCallbackSharedPtr ptr)
{
	nuulsServerReply(str, std::get<4>(*ptr));
}

void SaivBot::clipCommandCallback(std::string && str, ClipCallbackSharedPtr ptr)
{
	nuulsServerReply(str, *ptr);
}
void SaivBot::nuulsServerReply(const std::string & str, const IRCMessage & msg)
{
	std::stringstream reply;
	reply << msg.getNick() << ", " << str;
	sendPRIVMSG(msg.getParams()[0], reply.str());
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

std::size_t countTargetOccurrences(const std::string_view & str, const std::string_view & target)
{
	auto search = std::boyer_moore_searcher(target.begin(), target.end());
	return countTargetOccurrences(str.begin(), str.end(), search);
}

bool caselessCompare(const std::string_view & str1, const std::string_view & str2)
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
