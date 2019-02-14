//Log.cpp

#include "../include/Log.hpp"

Log::Log::Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, ParserFunc parser) :
	m_period(std::move(period)),
	m_channel_name(std::move(channel_name)),
	m_data(std::move(data))
{
	m_valid = parser(m_data, m_lines);
}

bool Log::Log::isValid() const
{
	return m_valid;
}

TimeDetail::TimePeriod Log::Log::getPeriod() const
{
	return m_period;
}

const std::string & Log::Log::getChannelName() const
{
	return m_channel_name;
}

const std::string & Log::Log::getData() const
{
	return m_data;
}

const std::vector<Log::LineView> & Log::Log::getLines() const
{
	return m_lines;
}

std::size_t Log::Log::getNumberOfLines() const
{
	return m_lines.size();
}
