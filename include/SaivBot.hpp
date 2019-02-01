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
//#include "IRCMessageTimedBuffer.hpp"
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
	void resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

	/*
	*/
	void connectHandler(boost::system::error_code ec);

	/*
	*/
	void handshakeHandler(boost::system::error_code ec);

	/*
	*/
	void receiveHandler(boost::system::error_code ec, std::size_t ret);
	
	/*
	*/
	void postSendIRC(std::string && msg);
	
	void doSendQueue();

	void onSendQueue(boost::beast::error_code ec, std::size_t bytes_transferred);

	/*
	*/
	//void sendTimerHandler(
	//	boost::system::error_code ec, 
	//	std::shared_ptr<boost::asio::system_timer> timer_ptr, 
	//	std::shared_ptr<std::string> msg_ptr
	//);



	/*
	*/
	//void sendHandler(boost::system::error_code ec, std::size_t ret, std::shared_ptr<std::string> ptr);

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
	void sendPRIVMSG(const std::string_view & channel, const std::string_view & msg);

	/*
	Send JOIN.
	*/
	void sendJOIN(const std::string_view & channel);

	/*
	Send PART.
	*/
	void sendPART(const std::string_view & channel);

	/*
	Send "CAP REQ :twitch.tv/commands"
	*/	
	void sendWHISPERRequest();

	/*
	Send WHISPER
	*/
	void sendWHISPER(const std::string_view & target, const std::string_view & msg);

	/*
	Reply to IRCMessage.
	If message is from channel, then reply in that channel.
	If message is a whisper, then reply to whisper.
	*/
	void replyToIRCMessage(const IRCMessage & msg, const std::string_view & reply);

	/*
	Check if user is moderator.
	*/
	bool isModerator(const std::string_view & user);

	/*
	Check if user is whitelisted.
	*/
	bool isWhitelisted(const std::string_view & user);

	/////
	bool m_running = false;

	boost::asio::io_context & m_ioc;
	boost::asio::ssl::context m_ctx;
	boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_stream;
	boost::asio::ip::tcp::resolver m_resolver;
	std::filesystem::path m_config_path;
	
	boost::asio::io_context::strand m_send_strand;
	std::queue<std::string> m_send_queue;
	bool m_send_queue_busy = false;
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
	constexpr CommandContainer::FuncType  bindCommand(FuncPtr func)
	{
		return std::bind(func, this, std::placeholders::_1, std::placeholders::_2);
	}

	enum Commands
	{
		shutdown,
		help_command,
		count_command,
		//search_command,
		find_command,
		//regexfind_command,
		clip_command,
		promote_command,
		demote_command,
		join_command,
		part_command,
		uptime_command,
		say_command,
		ping_command,
		NUMBER_OF_COMMANDS
	};

	/*
	Command functions.
	*/
	void shutdownCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void helpCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void countCommandFunc(const IRCMessage & msg, std::string_view input_line);
	//void searchCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void findCommandFunc(const IRCMessage & msg, std::string_view input_line);
	//void regexfindCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void clipCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void promoteCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void demoteCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void joinCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void partCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void uptimeCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void sayCommandFunc(const IRCMessage & msg, std::string_view input_line);
	void pingCommandFunc(const IRCMessage & msg, std::string_view input_line);

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
		CommandContainer("ping", "", "Ping the bot", bindCommand(&SaivBot::pingCommandFunc))
	};
	
	/*
	*/
	using CountCallbackSharedPtr = std::shared_ptr<
		std::tuple<
			std::mutex, 
			std::size_t, 
			std::string,
			std::function<bool(char, char)>,
			IRCMessage,
			TimeDetail::TimePeriod,
			std::size_t,
			bool
		>>;
	void countCommandCallback(
		Log && log,
		CountCallbackSharedPtr ptr
	);

	/*
	*/
	using FindCallbackSharedPtr = std::shared_ptr<
		std::tuple<
			std::mutex,
			std::size_t,
			std::string,
			std::function<bool(char, char)>,
			IRCMessage,
			TimeDetail::TimePeriod,
			std::vector<Log>,
			std::vector<std::vector<std::reference_wrapper<const Log::LineView>>>,
			bool
		>>;
	void findCommandCallback(
		Log && log,
		FindCallbackSharedPtr ptr
	);
	void findCommandCallback2(
		std::string && str,
		FindCallbackSharedPtr ptr
	);

	/*
	*/
	using ClipCallbackSharedPtr = std::shared_ptr<
		IRCMessage
	>;
	void clipCommandCallback(
		std::string && str,
		ClipCallbackSharedPtr ptr
	);

	/*
	*/
	//void linesFromNowCollector(std::size_t lines, )

	/*
	*/
	void nuulsServerReply(const std::string & str, const IRCMessage & msg);

	/*
	*/
	std::vector<date::year_month> periodToYearMonths(const TimeDetail::TimePeriod & period);
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
std::size_t countTargetOccurrences(const std::string_view & str, const std::string_view & target);

std::size_t countTargetOccurrences(const std::string_view & str, const std::regex & regex);

/*
Caseless compare.
*/
bool caselessCompare(const std::string_view & str1, const std::string_view & str2);

//to lower case string.
inline std::string toLowerCaseString(const std::string_view & source)
{
	std::string str;
	std::transform(source.begin(), source.end(), std::back_inserter(str), ::tolower);
	return str;
}

//Format string to twitch irc channel name format. 
inline std::string formatIRCChannelName(const std::string_view & source)
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
