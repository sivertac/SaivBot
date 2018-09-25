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
		m_msg_buffer.emplace_back(std::move(line));
	}
}


void SaivBot::consumeMsgBuffer()
{
	while (!m_msg_buffer.empty()) {
		auto & msg = m_msg_buffer.front();
		if (msg.getCommand() != "PRIVMSG") {
			if (msg.getCommand() == "PING") {
				sendIRC("PONG");
			}
		}
		else {
			//xD
			if (isInPrefixCaseless(msg.getBody(), std::string_view("!xd"))) {
				sendPRIVMSG(msg.getParams()[0], "xD");
			}
			else if (isInPrefixCaseless(msg.getBody(), std::string_view("!NaM"))) {
				sendPRIVMSG(msg.getParams()[0], "NaM");
			}
			else if (msg.getBody().find("A multi-raffle has begun") != msg.getBody().npos) {
				sendPRIVMSG(msg.getParams()[0], "!join");
			}
			
			//commands
			{
				std::string_view local_view = msg.getBody();
				
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
								command_it->m_func(msg, local_view);
							}			
						}
					}
				}
			}
		}
		std::cout << msg.getData() << "\n";
		m_msg_buffer.pop_front();
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
			Option<ListType>("-year"),
			Option<WordType>("-year"),
			Option<ListType>("-month"),
			Option<WordType>("-month"),
			Option<>("-caseless")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		std::string channel;
		std::string user;

		std::function<bool(char, char)> predicate;

		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));

		std::vector<date::year> years;
		std::vector<date::month> months;

		//boost::posix_time::

		if (auto r = set.find<0>()) {
			search_str = r->get<0>();
		}
		else if (auto r = set.find<1>()) {
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

		if (auto r = set.find<4>()) { //year list
			auto list = r->get<0>();
			for (auto & e : list) {
				if (auto year = TimeDetail::parseYearString(e)) {
					years.push_back(*year);
				}
				else {
					sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid year. NaM"));
					return;
				}
			}
		}
		else if (auto r = set.find<5>()) { //year word
			auto word = r->get<0>();
			if (auto year = TimeDetail::parseYearString(word)) {
				years.push_back(*year);
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid year. NaM"));
				return;
			}
		}
		else {
			years.push_back(current_date.year());
		}

		if (auto r = set.find<6>()) { //month list
			auto list = r->get<0>();
			for (auto & e : list) {
				if (auto month = TimeDetail::parseMonthString(e)) {
					months.push_back(*month);
				}
				else {
					sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid month. NaM"));
					return;
				}
			}
		}
		else if (auto r = set.find<7>()) { //month word
			auto word = r->get<0>();
			if (auto month = TimeDetail::parseMonthString(word)) {
				months.push_back(*month);
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid month. NaM"));
				return;
			}
		}
		else {
			months.push_back(current_date.month());
		}

		if (auto r = set.find<8>()) {
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		if (channel[0] == '#') channel.erase(0, 1);
		
		const std::string host("api.gempir.com");
		const std::string port("443");

		CountCallbackSharedPtr callback_ptr = std::make_shared<CountCallbackSharedPtr::element_type>();
		std::get<1>(*callback_ptr) = years.size() * months.size();
		std::get<2>(*callback_ptr) = std::move(search_str);
		std::get<3>(*callback_ptr) = predicate;
		std::get<4>(*callback_ptr) = msg;
		
		for (auto & y : years) {
			for (auto & m : months) {
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
					m,
					y
				);
			}
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
			Option<ListType>("-year"),
			Option<WordType>("-year"),
			Option<ListType>("-month"),
			Option<WordType>("-month"),
			Option<>("-caseless")
		);

		auto set = parser.parse(input_line);

		std::string search_str;
		std::string channel;
		std::string user;

		std::function<bool(char, char)> predicate;

		date::year_month_day current_date(date::floor<date::days>(std::chrono::system_clock::now()));

		std::vector<date::year> years;
		std::vector<date::month> months;

		if (auto r = set.find<0>()) {
			search_str = r->get<0>();
		}
		else if (auto r = set.find<1>()) {
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

		if (auto r = set.find<4>()) { //year list
			auto list = r->get<0>();
			for (auto & e : list) {
				if (auto year = TimeDetail::parseYearString(e)) {
					years.push_back(*year);
				}
				else {
					sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid year. NaM"));
					return;
				}
			}
		}
		else if (auto r = set.find<5>()) { //year word
			auto word = r->get<0>();
			if (auto year = TimeDetail::parseYearString(word)) {
				years.push_back(*year);
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid year. NaM"));
				return;
			}
		}
		else {
			years.push_back(current_date.year());
		}

		if (auto r = set.find<6>()) { //month list
			auto list = r->get<0>();
			for (auto & e : list) {
				if (auto month = TimeDetail::parseMonthString(e)) {
					months.push_back(*month);
				}
				else {
					sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid month. NaM"));
					return;
				}
			}
		}
		else if (auto r = set.find<7>()) { //month word
			auto word = r->get<0>();
			if (auto month = TimeDetail::parseMonthString(word)) {
				months.push_back(*month);
			}
			else {
				sendPRIVMSG(msg.getParams()[0], std::string(msg.getNick()).append(", invalid month. NaM"));
				return;
			}
		}
		else {
			months.push_back(current_date.month());
		}

		if (auto r = set.find<8>()) {
			predicate = [](char l, char r) {return std::tolower(l) == std::tolower(r); };
		}
		else {
			predicate = std::equal_to<char>();
		}

		if (channel[0] == '#') channel.erase(0, 1);

		const std::string host("api.gempir.com");
		const std::string port("443");

		FindCallbackSharedPtr callback_ptr = std::make_shared<FindCallbackSharedPtr::element_type>();
		std::get<1>(*callback_ptr) = years.size() * months.size();
		std::get<2>(*callback_ptr) = std::move(search_str);
		std::get<3>(*callback_ptr) = predicate;
		std::get<4>(*callback_ptr) = msg;

		for (auto & y : years) {
			for (auto & m : months) {
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
					m,
					y
				);
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
	const IRCMessage & msg = std::get<4>(*ptr);
	std::vector<Log> & log_container = std::get<5>(*ptr);
	{
		std::lock_guard<std::mutex> lock(mutex);
		--mutex_count;
		log_container.push_back(std::move(log));
		if (mutex_count == 0) {
			auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
			//auto searcher = std::boyer_moore_searcher(search_str.begin(), search_str.end(), std::hash<char>(), predicate);
			//auto searcher = std::boyer_moore_horspool_searcher(search_str.begin(), search_str.end(), std::hash<char>(), predicate);
			std::size_t count = 0;
			for (auto & l : log_container) {
				if (l.getData() == "{\"message\":\"Not Found\"}") {
					continue;
				}
				else {
					for (std::size_t i = 0; i < l.getNumberOfLines(); ++i) {
						auto & msg = l.getMessages()[i];
						count += countTargetOccurrences(msg.begin(), msg.end(), searcher);	
					}
				}
			}
			std::stringstream reply;
			reply << msg.getNick() << ", count: " << count << ", logs: " << log_container.size();
			sendPRIVMSG(msg.getParams()[0], reply.str());
		}
	}
}

void SaivBot::findCommandCallback(Log && log, FindCallbackSharedPtr ptr)
{
	std::mutex & mutex = std::get<0>(*ptr);
	std::size_t & mutex_count = std::get<1>(*ptr);
	std::string & search_str = std::get<2>(*ptr);
	auto predicate = std::get<3>(*ptr);
	const IRCMessage & msg = std::get<4>(*ptr);
	std::vector<Log> & log_container = std::get<5>(*ptr);

	{
		std::lock_guard<std::mutex> lock(mutex);
		--mutex_count;
		log_container.push_back(std::move(log));
		if (mutex_count == 0) {
			std::stringstream lines_found_stream;
			auto searcher = std::default_searcher(search_str.begin(), search_str.end(), predicate);
			std::size_t count = 0;
			for (auto & l : log_container) {
				if (l.getData() == "{\"message\":\"Not Found\"}") {
					continue;
				}
				else {
					for (std::size_t i = 0; i < l.getNumberOfLines(); ++i) {
						auto & time = l.getTimes()[i];
						auto & msg = l.getMessages()[i];
						if (std::search(msg.begin(), msg.end(), searcher) != msg.end()) {
							using namespace date;
							lines_found_stream << time << ": " << msg << "\n";
							++count;
						}
					}
				}
			}
			if (count > 0) {
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
		}
	}
}

void SaivBot::findCommandCallback2(std::string && str, FindCallbackSharedPtr ptr)
{
	const IRCMessage & msg = std::get<4>(*ptr);
	std::stringstream reply;
	reply << msg.getNick() << ", " << str;
	sendPRIVMSG(msg.getParams()[0], reply.str());
}

/*
void SaivBot::countCommandCallback(Log && log, std::shared_ptr<std::string> search_ptr, std::shared_ptr<std::string> channel_ptr, std::shared_ptr<std::string> nick_ptr)
{
	std::stringstream reply;
	std::size_t count = 0;
	auto searcher = std::boyer_moore_searcher(search_ptr->begin(), search_ptr->end());
	if (log.getData() == "{\"message\":\"Not Found\"}") {
		reply << *nick_ptr << ", no logs found NaM";
	}
	else {
		for (std::size_t i = 0; i < log.getNumberOfLines(); ++i) {
			auto & msg = log.getLines()[i];
			count += countTargetOccurrences(msg.begin(), msg.end(), searcher);
		}
		reply << *nick_ptr << ", count: " << count;
	}
	sendPRIVMSG(*channel_ptr, reply.str());
}
*/

void IRCMessage::parse()
{
	std::string_view data_view(m_data);
	std::size_t space = 0;

	//prefix
	if (data_view[0] == ':') {
		space = data_view.find_first_of(' ');
		std::string_view prefix_view = data_view.substr(1, space);
		
		std::size_t at = prefix_view.find_first_of('@');
		if (at != prefix_view.npos) {
			std::size_t ex = prefix_view.find_first_of('!');
			if (ex != prefix_view.npos) {
				m_nick_view = prefix_view.substr(0, ex);
				m_user_view = prefix_view.substr(ex + 1, at - ex - 1);
				m_host_view = prefix_view.substr(at + 1);
			}
			else {
				m_nick_view = prefix_view.substr(0, at);
				m_host_view = prefix_view.substr(at + 1);
			}
		}
		else {
			m_nick_view = prefix_view;
		}
		
		data_view.remove_prefix(space + 1);
		if (data_view.empty()) return;
	}
	
	//command
	{
		space = data_view.find_first_of(' ');
		m_command_view = data_view.substr(0, space);
		
		data_view.remove_prefix(space + 1);
		if (data_view.empty()) return;
	}

	//params
	{
		while (!data_view.empty()) {
			if (data_view[0] == ':') break;
			space = data_view.find_first_of(' ');
			if (space == data_view.npos) {
				m_params_vec.push_back(data_view);
				data_view.remove_prefix(data_view.size());
				break;
			}
			else {
				m_params_vec.push_back(data_view.substr(0, space));
				data_view.remove_prefix(space + 1);
			}
		}
		if (data_view.empty()) return;
	}

	//body
	if (data_view[0] == ':') {
		m_body_view = data_view.substr(1);
	}
}

void IRCMessage::print(std::ostream & stream)
{
	stream
		<< "nick: " << m_nick_view << "\n"
		<< "user: " << m_user_view << "\n"
		<< "host: " << m_host_view << "\n"
		<< "command: " << m_command_view << "\n"
		<< "params: " << [](auto & vec)->std::string {std::string str; std::for_each(vec.begin(), vec.end(), [&](auto & s) {str.append(std::string(s) + ", "); }); return str; }(m_params_vec) << "\n"
		<< "body: " << m_body_view << "\n";
}

IRCMessage::IRCMessage(const IRCMessage & source)
{
	//copy data string
	m_data = source.m_data;

	//create new string_views relative to m_data
	{
		auto & source_view = source.m_nick_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_nick_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_user_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_user_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_host_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_host_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_command_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_command_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_vec = source.m_params_vec;
		m_params_vec.reserve(source_vec.size());
		for (auto & e : source_vec) {
			assert(e.data() >= source.m_data.data());
			std::ptrdiff_t distance = e.data() - source.m_data.data();
			m_params_vec.emplace_back(m_data.data() + distance, e.size());
		}
	}
	{
		auto & source_view = source.m_body_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_body_view = std::string_view(m_data.data() + distance, source_view.size());
	}
}

IRCMessage & IRCMessage::operator=(const IRCMessage & source)
{
	//copy data string
	m_data = source.m_data;

	//create new string_views relative to m_data
	{
		auto & source_view = source.m_nick_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_nick_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_user_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_user_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_host_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_host_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_view = source.m_command_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_command_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	{
		auto & source_vec = source.m_params_vec;
		m_params_vec.reserve(source_vec.size());
		for (auto & e : source_vec) {
			assert(e.data() >= source.m_data.data());
			std::ptrdiff_t distance = e.data() - source.m_data.data();
			m_params_vec.emplace_back(m_data.data() + distance, e.size());
		}
	}
	{
		auto & source_view = source.m_body_view;
		assert(source_view.data() >= source.m_data.data());
		std::ptrdiff_t distance = source_view.data() - source.m_data.data();
		m_body_view = std::string_view(m_data.data() + distance, source_view.size());
	}
	return *this;
}

const std::string & IRCMessage::getData() const
{
	return m_data;
}

const std::string_view & IRCMessage::getNick() const
{
	return m_nick_view;
}

const std::string_view & IRCMessage::getUser() const
{
	return m_user_view;
}

const std::string_view & IRCMessage::getHost() const
{
	return m_host_view;
}

const std::string_view & IRCMessage::getCommand() const
{
	return m_command_view;
}

const std::vector<std::string_view>& IRCMessage::getParams() const
{
	return m_params_vec;
}

const std::string_view & IRCMessage::getBody() const
{
	return m_body_view;
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
