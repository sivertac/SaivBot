//IRCMessageTimedBuffer.hpp
#pragma once
#ifndef IRCMessageTimedBuffer_HEADER
#define IRCMessageTimedBuffer_HEADER

//C++
#include <iostream>
#include <mutex>
#include <deque>
#include <chrono>
#include <functional>
#include <cassert>

//boost
#include <boost/asio.hpp>

//local
#include "IRCMessage.hpp"

class IRCMessageTimedBuffer
{
public:
	using Container = std::deque<IRCMessage>;
	using Duration = std::chrono::system_clock::duration;

	IRCMessageTimedBuffer(boost::asio::io_context & ioc, const Duration & timeout);

	void push(IRCMessage && msg);

	void accessData(std::function<void(const Container&)> func);
private:
	void timerHandler(boost::system::error_code ec);

	boost::asio::io_context & m_ioc;
	std::mutex m_mutex;
	Duration m_timeout;
	boost::asio::system_timer m_timer;
	Container m_queue;
};

#endif // !IRCMessageTimedBuffer_HEADER
