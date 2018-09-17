//LogDownloader.cpp

#include "../include/LogDownloader.hpp"

Log::Log(const boost::posix_time::time_period & period, std::string && data) :
	m_period(period),
	m_data(std::move(data))
{
	parse();
}

const boost::posix_time::time_period & Log::getPeriod()
{
	return m_period;
}

const std::string & Log::getData() const
{
	return m_data;
}

const std::vector<std::string_view> & Log::getLines() const
{
	return m_lines;
}

std::size_t Log::getNumberOfLines() const
{
	return m_lines.size();
}

const std::vector<boost::posix_time::ptime>& Log::getTimes() const
{
	return m_times;
}

const std::vector<std::string_view>& Log::getNames() const
{
	return m_names;
}

const std::vector<std::string_view>& Log::getMessages() const
{
	return m_messages;
}

void Log::parse()
{


	const std::string_view cr("\r\n");
	std::string_view data_view(m_data);
	while (!data_view.empty()) {
		auto cr_pos = data_view.find(cr);
		if (cr_pos == data_view.npos) break;
		std::string_view line = data_view.substr(0, cr_pos + cr.size());
		std::string_view all = data_view.substr(0, cr_pos);



		auto space = all.find("] ");
		if (space == all.npos) break;

		std::string_view time = all.substr(1, space - 1);
		all.remove_prefix(space + 2);

		space = all.find(' ');
		if (space == all.npos) break;
		std::string_view name = all.substr(0, space - 1);

		all.remove_prefix(space + 1);
		std::string_view message = all;

		m_lines.push_back(line);
		m_times.push_back(boost::posix_time::time_from_string(std::string(time)));
		m_names.push_back(name);
		m_messages.push_back(message);

		//advance
		data_view.remove_prefix(cr_pos + cr.size());
	}
}
