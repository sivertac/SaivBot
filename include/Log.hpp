//Log.hpp
#pragma once
#ifndef Log_HEADER
#define Log_HEADER

//C++
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <chrono>

//local
#include "TimeDetail.hpp"

class Log
{
public:
	class LineView
	{
	public:
		LineView(
			const std::string_view & line_view,
			const std::string_view & time_view,
			const TimeDetail::TimePoint & time,
			const std::string_view & name_view,
			const std::string_view & message_view
		) :
			m_line_view(line_view),
			m_time_view(time_view),
			m_time(time),
			m_name_view(name_view),
			m_message_view(message_view)
		{
		}

		const std::string_view & getLineView() const
		{
			return m_line_view;
		}
		const std::string_view & getTimeView() const
		{
			return m_time_view;
		}
		const TimeDetail::TimePoint & getTime() const
		{
			return m_time;
		}
		const std::string_view & getNameView() const
		{
			return m_name_view;
		}
		const std::string_view & getMessageView() const
		{
			return m_message_view;
		}
	private:
		std::string_view m_line_view;
		std::string_view m_time_view;
		TimeDetail::TimePoint m_time;
		std::string_view m_name_view;
		std::string_view m_message_view;
	};

	using ParserFunc = std::function<bool(const std::string&, std::vector<LineView>&)>;

	//Log() = default;
	Log(const TimeDetail::TimePeriod & period, std::string && data, ParserFunc parser);

	bool isValid() const;

	TimeDetail::TimePeriod getPeriod() const;

	const std::string & getData() const;

	const std::vector<LineView> & getLines() const;

	std::size_t getNumberOfLines() const;

private:

	bool m_valid = false;
	std::string m_data;

	const TimeDetail::TimePeriod m_period;

	std::vector<LineView> m_lines;
};

#endif // !Log_HEADER
