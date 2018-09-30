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

void IRCMessageBuffer::accessData(std::function<void(const Container&)> func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	func(m_queue);
}
