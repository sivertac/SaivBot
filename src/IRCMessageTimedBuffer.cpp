//IRCMessageTimedBuffer.cpp

#include "../include/IRCMessageTimedBuffer.hpp"

IRCMessageTimedBuffer::IRCMessageTimedBuffer(boost::asio::io_context & ioc, const Duration & timeout) :
	m_ioc(ioc),
	m_timeout(timeout),
	m_timer(ioc)
{
}

void IRCMessageTimedBuffer::push(IRCMessage && msg)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_queue.empty()) {
		m_timer.expires_at(msg.getTime() + m_timeout);
		m_queue.push_back(std::move(msg));
		m_timer.async_wait(
			std::bind(
				&IRCMessageTimedBuffer::timerHandler,
				this,
				std::placeholders::_1
			)
		);
	}
	else {
		assert(m_queue.back().getTime() <= msg.getTime());
		m_queue.push_back(std::move(msg));
	}
}

void IRCMessageTimedBuffer::accessData(std::function<void(const Container&)> func)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	func(m_queue);
}

void IRCMessageTimedBuffer::timerHandler(boost::system::error_code ec)
{
	if (ec) throw std::runtime_error(ec.message());
	
	std::lock_guard<std::mutex> lock(m_mutex);

	m_queue.pop_front();
	//std::cout << "IRCMessageTimedBuffer: pop!!!\n";

	if (!m_queue.empty()) {
		m_timer.expires_at(m_queue.front().getTime() + m_timeout);
		m_timer.async_wait(
			std::bind(
				&IRCMessageTimedBuffer::timerHandler,
				this,
				std::placeholders::_1
			)
		);
	}
}
