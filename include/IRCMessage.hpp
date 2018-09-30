//IRCMessage.hpp
//Author: Sivert Andresen Cubedo
#pragma once
#ifndef IRCMessage_HEADER
#define IRCMessage_HEADER

//C++
#include <iostream>
#include <chrono>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>
#include <algorithm>

class IRCMessage
{
public:
	using TimePoint = std::chrono::system_clock::time_point;

	/*
	*/
	IRCMessage()
	{
	}
	IRCMessage(const TimePoint time, std::string && buf) :
		m_time(time),
		m_data(std::move(buf))
	{
		parseImplementation();
	}

	/*
	Copy.
	*/
	IRCMessage(const IRCMessage & source);
	IRCMessage & operator=(const IRCMessage & source);

	/*
	Move
	*/
	IRCMessage(IRCMessage && source) = default;
	IRCMessage & operator=(IRCMessage && source) = default;

	/*
	Getters.
	*/
	const TimePoint & getTime() const;
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
	*/
	void parseImplementation();

	/*
	*/
	void copyImplementation(const IRCMessage & source);

	TimePoint m_time;
	std::string m_data;
	std::string_view m_nick_view;
	std::string_view m_user_view;
	std::string_view m_host_view;
	std::string_view m_command_view;
	std::vector<std::string_view> m_params_vec;
	std::string_view m_body_view;
};

#endif
