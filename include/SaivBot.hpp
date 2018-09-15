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

#define _WIN32_WINNT 0x0A00

//boost
#include <boost\asio.hpp>

//json
#include <nlohmann\json.hpp>

//Local
#include "LogDownloader.hpp"
#include "OptionParser.hpp"

class IRCMessage
{
public:
	/*
	
	*/
	IRCMessage(std::string && buf) :
		m_data(std::move(buf))
	{
		parse();
	}

	/*
	Getters.
	*/
	const std::string & getData() const;
	const std::string_view & getNick() const;
	const std::string_view & getUser() const;
	const std::string_view & getHost() const;
	const std::string_view & getCommand() const;
	const std::vector<std::string_view> & getParams() const;
	const std::string_view & getBody() const;

	/*
	*/
	void print(std::ostream & stream);
	
private:
	/*
	Called by constructor.
	*/
	void parse();

	std::string m_data;
	std::string_view m_nick_view;
	std::string_view m_user_view;
	std::string_view m_host_view;
	std::string_view m_command_view;
	std::vector<std::string_view> m_params_vec;
	std::string_view m_body_view;
};

/*
Command container.
*/
struct CommandContainer
{
	using IteratorType = std::vector<std::string>::const_iterator;

	using FuncType = std::function<
		void(const IRCMessage&,
			IteratorType,
			IteratorType)>;


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
	SaivBot(boost::asio::io_context & ioc);


	/*
	Load config.
	*/
	void loadConfig(const std::filesystem::path & path);

	/*
	Save config.
	*/
	void saveConfig(const std::filesystem::path & path);
	
	/*
	Post to ioc.
	*/
	void run();
private:
	/*
	*/
	void resolveHandler(boost::system::error_code ec, boost::asio::ip::tcp::resolver::results_type results);

	/*
	*/
	void connectHandler(boost::system::error_code ec);

	/*
	*/
	void receiveHandler(boost::system::error_code ec, std::size_t ret);
	
	/*
	*/
	void sendIRC(const std::string & msg);
	
	/*
	*/
	void sendTimerHandler(
		boost::system::error_code ec, 
		std::shared_ptr<boost::asio::system_timer> timer_ptr, 
		std::shared_ptr<std::string> msg_ptr
	);

	/*
	*/
	void sendHandler(boost::system::error_code ec, std::size_t ret, std::shared_ptr<std::string> ptr);

	/*
	Parse buffer stream.
	*/
	void parseBuffer();

	/*
	*/
	void consumeMsgBuffer();

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
	Check if user is moderator.
	*/
	bool isModerator(const std::string_view & user);

	/*
	Check if user is whitelisted.
	*/
	bool isWhitelisted(const std::string_view & user);

	/*
	Caseless compare.
	*/
	static bool caselessCompare(const std::string_view & str1, const std::string_view & str2);

	/////
	bool m_running = false;

	boost::asio::io_context & m_ioc;
	boost::asio::ip::tcp::resolver m_resolver;
	boost::asio::ip::tcp::socket m_sock;

	std::mutex m_send_mutex;
	std::string m_last_message_queued;
	std::chrono::system_clock::time_point m_next_message_time;

	static const std::size_t m_buffer_size = 4000;
	std::array<char, m_buffer_size> m_recv_buffer;

	std::string m_buffer;
	std::deque<IRCMessage> m_msg_buffer;

	std::string m_host;
	std::string m_port;
	std::string m_nick;
	std::string m_password;

	std::unordered_set<std::string> m_whitelist;
	std::unordered_set<std::string> m_modlist;

	/*
	Bind command
	*/
	template <typename FuncPtr>
	auto bindCommand(FuncPtr func)
	{
		return std::bind(func, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
	}

	enum Commands
	{
		shutdown = 0,
		help_command,
		count_command,
		promote_command,
		demote_command,
		join_command,
		part_command,
		NUMBER_OF_COMMANDS
	};

	/*
	Command functions.
	*/
	void shutdownCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void helpCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void countCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void promoteCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void demoteCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void joinCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);
	void partCommandFunc(const IRCMessage & msg, CommandContainer::IteratorType begin, CommandContainer::IteratorType end);

	const std::array<CommandContainer, Commands::NUMBER_OF_COMMANDS> m_command_containers
	{
		CommandContainer("shutdown", "", "Orderly shut down execution and save config.", bindCommand(&SaivBot::shutdownCommandFunc)),
		CommandContainer("help", "<command>", "Get info about command.", bindCommand(&SaivBot::helpCommandFunc)),
		CommandContainer("count", "<target> [-flag1 [param ...] -flag2 [param ...] ...]", "Count the occurrences of target in log.", bindCommand(&SaivBot::countCommandFunc)),
		CommandContainer("promote", "<user>", "Whitelist user.", bindCommand(&SaivBot::promoteCommandFunc)),
		CommandContainer("demote", "<user>", "Remove user from whitelist.", bindCommand(&SaivBot::demoteCommandFunc)),
		CommandContainer("join", "<channel>", "Join channel.", bindCommand(&SaivBot::joinCommandFunc)),
		CommandContainer("part", "<channel>", "Part channel.", bindCommand(&SaivBot::partCommandFunc))
	};

	/*
	Count command callback.
	*/
	void countCommandCallback(
		LogDownloader::LogContainer && logs,
		std::shared_ptr<std::string> search_ptr,
		std::shared_ptr<std::string> channel_ptr,
		std::shared_ptr<std::string> nick_ptr
	);
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
	false false if not
*/
template <class C1, class C2>
bool prefixCompare(C1 & a, C2 & b)
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
Count show many times search occurs in str.
*/
template <typename Iterator, typename Searcher>
std::size_t countTargetOccurrences(Iterator begin, Iterator end, const Searcher & searcher)
{
	std::size_t count = 0;
	auto it = begin;
	while ((it = std::search(it, end, searcher)) != end) {
		++count;
		++it;
	}
	return count;
}
std::size_t countTargetOccurrences(const std::string_view & str, const std::string_view & target);

/*
Split string by spaces.
*/
std::vector<std::string> splitString(const std::string & str);

#endif // !SaivBot_HEADER
