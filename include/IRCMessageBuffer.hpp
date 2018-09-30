//IRCMessageBuffer.hpp
#pragma once
#ifndef IRCMessageBuffer_HEADER
#define IRCMessageBuffer_HEADER

//C++
#include <iostream>
#include <mutex>
#include <deque>
#include <functional>
#include <cassert>

//local
#include "IRCMessage.hpp"

class IRCMessageBuffer
{
public:
	using Container = std::deque<IRCMessage>;

	IRCMessageBuffer(std::size_t capacity);

	void push(IRCMessage && msg);

	/*
	*/
	using AccessDataFunc = std::function<void(const Container&)>;
	void accessData(AccessDataFunc func);

	/*
	*/
	using LinesFromNowVecType = std::vector<std::reference_wrapper<const IRCMessage>>;
	using LinesFromNowFuncType = std::function<void(const LinesFromNowVecType&)>;
	void accessLinesFromNow(std::size_t line_count, LinesFromNowFuncType func);
private:
	std::mutex m_mutex;
	std::size_t m_capacity;
	Container m_queue;
};

#endif // !IRCMessageBuffer_HEADER
