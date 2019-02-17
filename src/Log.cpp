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

void Log::Log::moveImpl(Log && source)
{
	m_valid = std::move(source.m_valid);
	m_channel_name = std::move(source.m_channel_name);
	m_period = std::move(source.m_period);
	m_data = std::move(source.m_data);
	m_lines = std::move(source.m_lines);
	if (source.m_data.size() <= sizeof(decltype(source.m_data))) {
		//handle SSO
		for (auto & line : m_lines) {
			line = LineView::copyView(source.m_data.data(), m_data.data(), line);
		}
	}
}
