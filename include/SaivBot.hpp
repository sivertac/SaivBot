//SaivBot.hpp
//Author: Sivert Andresen Cubedo
#pragma once
#ifndef SaivBot_HEADER
#define SaivBot_HEADER

//C++
#include <string>
#include <string_view>
#include <iostream>
#include <sstream>
#include <thread>
#include <atomic>
#include <array>
#include <memory>
#include <queue>
#include <deque>
#include <algorithm>
#include <chrono>
#include <atomic>
#include <fstream>
#include <filesystem>
#include <unordered_set>
#include <unordered_map>
#include <charconv>
#include <regex>
#include <set>

//Date
#include <date/date.h>

//boost
#include <boost/asio.hpp>

//json
#include <nlohmann/json.hpp>

//OptionParser
#include <OptionParser.hpp>

//Local
#include "IRCMessage.hpp"
#include "LogDownloader.hpp"
#include "DankHttp.hpp"
#include "IRCMessageBuffer.hpp"

/*
Command container.
*/
struct CommandContainer
{
	using FuncType = std::function<void(const IRCMessage&, std::string_view)>;

	CommandContainer(
		const std::string & command,
		const std::string & arguments,
		const std::string & description,
		const FuncType & func
	) :
		m_command(command),
		m_arguments(arguments),
		m_description(description),
		m_func(func)
	{
	}
	const std::string m_command;
	const std::string m_arguments;
	const std::string m_description;
	const FuncType m_func;
};

class SaivBot
{
public:
	/*
	*/
	SaivBot(
		boost::asio::io_context & ioc,
		boost::asio::ssl::context && ctx,
		const std::filesystem::path & config_path
	);
	
	/*
	Post to ioc.
	*/
	void run();
	
	/*
	*/
	~SaivBot();
private:

	/*
	Load config.
	*/
	void loadConfig(const std::filesystem::path & path);

	/*
	Save config.
	*/
	void saveConfig(const std::filesystem::path & path);

	/*
	*/
	void onResolve(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

	/*
	*/
	void onConnect(boost::system::error_code ec);

	/*
	*/
	void onHandshake(boost::system::error_code ec);

	/*
	*/
	void onRead(boost::system::error_code ec, std::size_t bytes_transferred);
	
	/*
	*/
	void postSendIRC(std::string && msg);
	
	void doSendQueue();

	void onSendQueue(boost::beast::error_code ec, std::size_t bytes_transferred);

	/*
	Parse buffer stream.
	*/
	void parseBuffer();

	/*
	*/
	void consumeMsgBuffer();

	/*
	*/
	void parseFreeMessage(const IRCMessage & msg);

	/*
	Send PRIVMSG.
	*/
	void sendPRIVMSG(std::string_view channel, std::string_view msg);

	/*
	Send JOIN.
	*/
	void sendJOIN(std::string_view channel);

	/*
	Send PART.
	*/
	void sendPART(std::string_view channel);

	/*
	Send "CAP REQ :twitch.tv/commands"
	*/	
	void sendWHISPERRequest();

	/*
	Send WHISPER
	*/
	void sendWHISPER(std::string_view target, std::string_view msg);

	/*
	Reply to IRCMessage.
	If message is from channel, then reply in that channel.
	If message is a whisper, then reply to whisper.
	*/
	void replyToIRCMessage(const IRCMessage & msg, std::string_view reply);

	void doShutdown();

	void postDoRECONNECT();

	void reconnectHandler();

	/*
	Check if user is moderator.
	*/
	bool isModerator(std::string_view user);

	/*
	Check if user is whitelisted.
	*/
	bool isWhitelisted(std::string_view user);

	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_stream;
	boost::asio::ip::tcp::resolver m_resolver;
	std::filesystem::path m_config_path;
	
	boost::asio::io_context::strand m_read_strand;
	boost::asio::io_context::strand m_send_strand;
	bool m_suspend_read = true;
	bool m_suspend_send = true;
	bool m_send_queue_busy = false;
	std::queue<std::string> m_send_queue;
	std::string m_last_message_sendt;
	std::chrono::system_clock::time_point m_last_message_sendt_time;
	std::chrono::system_clock::duration m_send_message_duration = std::chrono::milliseconds(1500);
	boost::asio::system_timer m_send_message_timer;

	static const std::size_t m_buffer_size = 10000;
	std::array<char, m_buffer_size> m_recv_buffer;

	std::string m_buffer;
	std::deque<IRCMessage> m_msg_pre_buffer;

	const std::size_t m_message_buffer_size = 1000;
	using ChannelData = std::tuple<std::unique_ptr<IRCMessageBuffer>>;
	std::unordered_map<std::string, ChannelData> m_channels;
	
	std::string m_host;
	std::string m_port;
	std::string m_nick;
	std::string m_password;

	TimeDetail::TimePoint m_time_started;

	std::unordered_set<std::string> m_whitelist;
	std::unordered_set<std::string> m_modlist;

	/*
	Bind command
	*/
	template <typename FuncPtr>
	constexpr CommandContainer::FuncType bindCommand(FuncPtr func)
	{
		return std::bind(func, this, std::placeholders::_1, std::placeholders::_2);
	}

	enum Commands
	{
		shutdown,
		help_command,
		count_command,
		find_command,
		clip_command,
		promote_command,
		demote_command,
		join_command,
		part_command,
		uptime_command,
		say_command,
		ping_command,
		commands_command,
		flags_command,
		test_insertmessage_command,
		NUMBER_OF_COMMANDS
	};

	/*
	Command functions.
	*/
	void shutdownCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void helpCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void countCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void findCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void clipCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void promoteCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void demoteCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void joinCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void partCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void uptimeCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void sayCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void pingCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void commandsCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void flagsCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void test_insertmessageCommandFunc(const IRCMessage & msg, std::string_view input_line);

	const std::array<CommandContainer, static_cast<std::size_t>(Commands::NUMBER_OF_COMMANDS)> m_command_containers
	{
		CommandContainer("shutdown", "", "Orderly shut down execution and save config.", bindCommand(&SaivBot::shutdownCommandFunc)),
		CommandContainer("help", "<command>", "Get info about command.", bindCommand(&SaivBot::helpCommandFunc)),
		CommandContainer("count", "<target> [-flag1 [param ...] -flag2 [param ...] ...]", "Count the occurrences of target in logs.", bindCommand(&SaivBot::countCommandFunc)),
		//CommandContainer("search", "<target> [-flag1 [param ...] -flag2 [param ...] ...]", "Search for target in logs", bindCommand(&SaivBot::searchCommandFunc)),
		CommandContainer("find", "<target> [-flag1 [param ...] -flag2 [param ...] ...]", "Find all lines containing target in logs.", bindCommand(&SaivBot::findCommandFunc)),
		//CommandContainer("regexfind", "<regex> [-flag1 [param ...] -flag2 [param ...] ...]", "Find regex matches in logs.", bindCommand(&SaivBot::regexfindCommandFunc)),
		CommandContainer("clip", "[-flag1 [param ...] -flag2 [param ...] ...]", "Capture a snapshot of chat.", bindCommand(&SaivBot::clipCommandFunc)),
		CommandContainer("promote", "<user>", "Whitelist user.", bindCommand(&SaivBot::promoteCommandFunc)),
		CommandContainer("demote", "<user>", "Remove user from whitelist.", bindCommand(&SaivBot::demoteCommandFunc)),
		CommandContainer("join", "<channel>", "Join channel.", bindCommand(&SaivBot::joinCommandFunc)),
		CommandContainer("part", "<channel>", "Part channel.", bindCommand(&SaivBot::partCommandFunc)),
		CommandContainer("uptime", "", "Get uptime.", bindCommand(&SaivBot::uptimeCommandFunc)),
		CommandContainer("say", "<stuff to say>", "Make bot say something.", bindCommand(&SaivBot::sayCommandFunc)),
		CommandContainer("ping", "", "Ping the bot", bindCommand(&SaivBot::pingCommandFunc)),
		CommandContainer("commands", "", "Get link to commands doc.", bindCommand(&SaivBot::commandsCommandFunc)),
		CommandContainer("flags", "", "Get link to flags doc.", bindCommand(&SaivBot::flagsCommandFunc)),
		CommandContainer("test_insertmessage", "<string>", "insert IRCMessage in receive queue.", bindCommand(&SaivBot::test_insertmessageCommandFunc))
	};

	void fillLogRequestTargetFields(
		LogRequest & log_request,
		const LogService service,
		const bool all_users,
		const TimeDetail::TimePeriod & period,
		const std::vector<std::string_view> & channels,
		const std::vector<std::string_view> & users
	)
	{
		auto generate_year_month_user_list = [&period, &channels, &users, this](auto func) -> std::vector<LogRequest::Target> {
			auto year_months = periodToYearMonths(period);
			std::vector<LogRequest::Target> vec;
			for (auto & channel : channels) {
				for (auto & user : users) {
					for (auto & ym : year_months) {
						vec.emplace_back(TimeDetail::createYearMonthPeriod(ym), channel, func(channel, user, ym));
					}
				}
			}
			return vec;
		};
		auto generate_date_list = [&period, &channels, this](auto func) -> std::vector<LogRequest::Target> {
			auto dates = periodToDates(period);
			std::vector<LogRequest::Target> vec;
			for (auto & channel : channels) {
				for (auto & date : dates) {
					vec.emplace_back(TimeDetail::createYearMonthDayPeriod(date), channel, func(channel, date));
				}
			}
			return vec;
		};
		if (service == LogService::gempir_log) {
			log_request.parser = gempirLogParser;
			log_request.host = "api.gempir.com";
			log_request.port = "443";
			if (!all_users) {
				log_request.targets = generate_year_month_user_list(createGempirUserTarget);
			}
			else {
				log_request.targets = generate_date_list(createGempirChannelTarget);
			}
		}
		else if (service == LogService::overrustle_log) {
			log_request.parser = overrustleLogParser;
			log_request.host = "overrustlelogs.net";
			log_request.port = "443";
			if (!all_users) {
				log_request.targets = generate_year_month_user_list(createOverrustleUserTarget);
			}
			else {
				log_request.targets = generate_date_list(createOverrustleChannelTarget);
			}
		}
		else {
			assert(false);
		}	
	}

	struct CountCallbackSharedData
	{
		std::mutex mutex;
		std::size_t reference_count;
		std::function<std::size_t(std::string_view)> count_func;
		TimeDetail::TimePeriod period;
		IRCMessage irc_msg;
		std::size_t shared_count;
	};

	void countCommandCallback(
		Log && log,
		std::shared_ptr<CountCallbackSharedData> shared_data_ptr
	)
	{
		std::size_t count = 0;
		try {
			if (log.isValid()) {
				for (auto & line : log.getLines()) {
					if (shared_data_ptr->period.isInside(line.getTime())) {
						count += shared_data_ptr->count_func(line.getMessageView());
					}
				}
			}
		}
		catch (std::exception)
		{
		}
		std::lock_guard<std::mutex> lock(shared_data_ptr->mutex);
		if (shared_data_ptr->reference_count > 0) {
			shared_data_ptr->shared_count += count;
			--shared_data_ptr->reference_count;
			if (shared_data_ptr->reference_count <= 0) {
				std::stringstream reply;
				reply << shared_data_ptr->irc_msg.getNick() << ", count: " << shared_data_ptr->shared_count;
				sendPRIVMSG(shared_data_ptr->irc_msg.getParams()[0], reply.str());
			}
		}
	}

	void countCommandErrorHandler(std::shared_ptr<CountCallbackSharedData> shared_data_ptr)
	{
		std::lock_guard<std::mutex> lock(shared_data_ptr->mutex);
		shared_data_ptr->reference_count = 0;
		std::stringstream reply;
		reply
			<< shared_data_ptr->irc_msg.getNick()
			<< ", "
			<< "Error while downloading log NaM";
		replyToIRCMessage(shared_data_ptr->irc_msg, reply.str());
	}

	struct FindCallbackSharedData
	{
		using ChannelSet = std::set<Log::ChannelName>;
		using SharedLinesFound = std::vector<std::tuple<ChannelSet::const_iterator, Log::Line>>;
		std::mutex mutex;
		std::size_t reference_count;
		std::function<bool(std::string_view)> find_func;
		std::function<std::string(SharedLinesFound&)> dump_func;
		TimeDetail::TimePeriod period;
		IRCMessage irc_msg;
		SharedLinesFound shared_lines_found;
		ChannelSet channels;
	};

	void findCommandCallback(
		Log && log,
		std::shared_ptr<FindCallbackSharedData> shared_data_ptr
	)
	{
		FindCallbackSharedData::SharedLinesFound lines_found;
		try {
			if (log.isValid()) {
				//find ChannelName ptr
				auto it = shared_data_ptr->channels.find(log.getChannelName());
				assert(it != shared_data_ptr->channels.end());
				for (auto & line : log.getLines()) {
					if (shared_data_ptr->period.isInside(line.getTime())) {
						if (shared_data_ptr->find_func(line.getMessageView())) {
							lines_found.emplace_back(it, line);
						}
					}
				}
			}
		}
		catch (std::exception) {
		}

		std::lock_guard<std::mutex> lock(shared_data_ptr->mutex);
		if (shared_data_ptr->reference_count > 0) {
			--shared_data_ptr->reference_count;
			if (!lines_found.empty()) {
				shared_data_ptr->shared_lines_found.insert(shared_data_ptr->shared_lines_found.end(), lines_found.begin(), lines_found.end());
			}
			if (shared_data_ptr->reference_count == 0) {
				if (!shared_data_ptr->shared_lines_found.empty()) {
					std::string str = shared_data_ptr->dump_func(shared_data_ptr->shared_lines_found);
					auto upload_handler = [irc_msg = std::move(shared_data_ptr->irc_msg), this](std::string && str) {
						nuulsServerReply(str, irc_msg);
					};
					std::make_shared<DankHttp::NuulsUploader>(m_ioc)->run(
						upload_handler,
						std::move(str),
						"i.nuuls.com",
						"443",
						"/upload?key=dank_password"
					);
				}
				else {
					std::stringstream reply;
					reply << shared_data_ptr->irc_msg.getNick() << ", no hit NaM";
					replyToIRCMessage(shared_data_ptr->irc_msg, reply.str());
				}
			}
		}
	}

	void findCommandErrorHandler(std::shared_ptr<FindCallbackSharedData> shared_data_ptr)
	{
		std::lock_guard<std::mutex> lock(shared_data_ptr->mutex);
		shared_data_ptr->reference_count = 0;
		std::stringstream reply;
		reply
			<< shared_data_ptr->irc_msg.getNick()
			<< ", "
			<< "Error while downloading log NaM";
		replyToIRCMessage(shared_data_ptr->irc_msg, reply.str());
	}

	/*
	*/
	using ClipCallbackSharedPtr = std::shared_ptr<
		IRCMessage
	>;
	void clipCommandCallback(
		std::string && str,
		ClipCallbackSharedPtr ptr
	);
	
	void nuulsServerReply(const std::string & str, const IRCMessage & msg);

	std::vector<date::year_month> periodToYearMonths(const TimeDetail::TimePeriod & period);
	
	std::vector<date::year_month_day> periodToDates(const TimeDetail::TimePeriod & period);
};

/*
Prefix compare.
Template:
	C				container
Parameters:
	a
	b
Return:
	true if a is a prefix of b, or b is a prefix of a
	false if not
*/
template <class C1, class C2>
bool prefixCompare(const C1 & a, const C2 & b)
{
	auto a_it = a.begin();
	auto b_it = b.begin();
	while (a_it != a.end() && b_it != b.end()) {
		if (*a_it != *b_it) {
			return false;
		}
		std::advance(a_it, 1);
		std::advance(b_it, 1);
	}
	return true;
}

/*

*/
template <class C1, class C2>
bool isInPrefixCaseless(const C1 & str, const C2 & sub)
{
	auto a_it = str.begin();
	auto b_it = sub.begin();
	while (a_it != str.end() && b_it != sub.end()) {
		if (std::tolower(*a_it) != std::tolower(*b_it)) {
			return false;
		}
		std::advance(a_it, 1);
		std::advance(b_it, 1);
		if (a_it == str.end() && b_it != sub.end()) return false;
		else if (b_it == sub.end()) return true;
	}
	return true;
}

/*
Count show many times search occurs in str.
*/
template <typename Iterator, typename Searcher>
std::size_t countTargetOccurrences(Iterator begin, Iterator end, const Searcher & searcher)
{
	std::size_t count = 0;
	auto it = begin;
	while ((it = std::search(it, end, searcher)) != end) {
		++count;
		std::advance(it, 1);
	}
	return count;
}
std::size_t countTargetOccurrences(std::string_view str, const std::regex & regex);

/*
Caseless compare.
*/
bool caselessCompare(std::string_view str1, std::string_view str2);

//to lower case string.
inline std::string toLowerCaseString(std::string_view source)
{
	std::string str;
	std::transform(source.begin(), source.end(), std::back_inserter(str), ::tolower);
	return str;
}

//Format string to twitch irc channel name format. 
inline std::string formatIRCChannelName(std::string_view source)
{
	if (source.empty()) return std::string("#");
	if (source[0] == '#') return toLowerCaseString(source);
	else {
		std::stringstream ss;
		ss << '#' << toLowerCaseString(source);
		return ss.str();
	}
}


#endif // !SaivBot_HEADER
