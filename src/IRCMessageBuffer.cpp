//IRCMessageBuffer.cpp

#include "../include/IRCMessageBuffer.hpp"

IRCMessageBuffer::IRCMessageBuffer(std::size_t size) :
	m_capacity(size)
{
	assert(size > 0);
}

void IRCMessageBuffer::push(IRCMessage && msg)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	m_queue.push_back(std::move(msg));
	if (m_queue.size() > m_capacity) {
		m_queue.pop_front();
		assert(m_queue.size() <= m_capacity);
	}
}

void IRCMessageBuffer::accessData(IRCMessageBuffer::AccessDataFunc func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	func(m_queue);
}

void IRCMessageBuffer::accessLinesFromNow(std::size_t line_count, LinesFromNowFuncType func)
{
	if (line_count == 0) return; //!!!

	accessData([&](const IRCMessageBuffer::Container & queue)
	{
		LinesFromNowVecType lines_from_now;
		lines_from_now.reserve(line_count);
		std::size_t j = line_count - 1;
		for (std::size_t i = 0; i < line_count; ++i) {
			if (queue.size() > i) {
				const IRCMessage & irc_msg = *(queue.crbegin() + i);
				lines_from_now.push_back(irc_msg);
			}
			else {
				break;
			}
		}
		std::reverse(lines_from_now.begin(), lines_from_now.end());
		func(lines_from_now);
	}
	);
}
