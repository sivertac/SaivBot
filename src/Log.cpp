//Log.cpp

#include "../include/Log.hpp"

Log::Log(const TimeDetail::TimePeriod & period, std::string && data, ParserFunc parser) :
	m_period(period),
	m_data(std::move(data))
{
	m_valid = parser(m_data, m_lines);	
}

bool Log::isValid() const
{
	return m_valid;
}

TimeDetail::TimePeriod Log::getPeriod() const
{
	return m_period;
}

const std::string & Log::getData() const
{
	return m_data;
}

const std::vector<Log::LineView> & Log::getLines() const
{
	return m_lines;
}

std::size_t Log::getNumberOfLines() const
{
	return m_lines.size();
}
