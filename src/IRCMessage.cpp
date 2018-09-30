//IRCMessage.cpp

#include "../include/IRCMessage.hpp"

void IRCMessage::parseImplementation()
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

void IRCMessage::copyImplementation(const IRCMessage & source)
{
	//copy time
	m_time = source.m_time;

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
	copyImplementation(source);
}

IRCMessage & IRCMessage::operator=(const IRCMessage & source)
{
	copyImplementation(source);
	return *this;
}

const IRCMessage::TimePoint & IRCMessage::getTime() const
{
	return m_time;
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
