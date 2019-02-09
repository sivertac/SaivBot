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
			std::string_view line_view,
			std::string_view time_view,
			TimeDetail::TimePoint time,
			std::string_view name_view,
			std::string_view message_view
		) :
			m_line_view(line_view),
			m_time_view(time_view),
			m_time(time),
			m_name_view(name_view),
			m_message_view(message_view)
		{
		}

		std::string_view getLineView() const
		{
			return m_line_view;
		}
		std::string_view getTimeView() const
		{
			return m_time_view;
		}
		TimeDetail::TimePoint getTime() const
		{
			return m_time;
		}
		std::string_view getNameView() const
		{
			return m_name_view;
		}
		std::string_view getMessageView() const
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

	class Line
	{
	public:
		Line(const LineView & line_view)
		{
			copyLineViewImpl(line_view);
		}

		Line(const Line & source)
		{
			copyLineImpl(source);
		}
		Line & operator=(const Line & source)
		{
			copyLineImpl(source);
			return *this;
		}

		Line(Line && source) = default;
		Line & operator=(Line && source) = default;

		std::string getLine() const
		{
			return m_line;
		}
		std::string_view getTimeView() const
		{
			return m_time_view;
		}
		TimeDetail::TimePoint getTime() const
		{
			return m_time;
		}
		std::string_view getNameView() const
		{
			return m_name_view;
		}
		std::string_view getMessageView() const
		{
			return m_message_view;
		}
	private:
		std::string m_line;
		std::string_view m_time_view;
		TimeDetail::TimePoint m_time;
		std::string_view m_name_view;
		std::string_view m_message_view;

		void copyLineViewImpl(const LineView & line_view)
		{
			m_time = line_view.getTime();
			m_line = line_view.getLineView();
			auto copy_view = [&](std::string_view source_view) {
				assert(source_view.data() >= line_view.getLineView().data());
				std::ptrdiff_t distance = source_view.data() - line_view.getLineView().data();
				return std::string_view(m_line.data() + distance, source_view.size());
			};
			m_time_view = copy_view(line_view.getTimeView());
			m_name_view = copy_view(line_view.getNameView());
			m_message_view = copy_view(line_view.getMessageView());
		}
		
		void copyLineImpl(const Line & source)
		{
			m_time = source.m_time;
			m_line = source.m_line;
			auto copy_view = [&](std::string_view source_view) {
				assert(source_view.data() >= source.m_line.data());
				std::ptrdiff_t distance = source_view.data() - source.m_line.data();
				return std::string_view(m_line.data() + distance, source_view.size());
			};
			m_time_view = copy_view(source.m_time_view);
			m_name_view = copy_view(source.m_name_view);
			m_message_view = copy_view(source.m_message_view);
		}
	};

	using ParserFunc = std::function<bool(const std::string&, std::vector<LineView>&)>;
	using ChannelName = std::string;

	//Log() = default;
	Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, ParserFunc parser);

	bool isValid() const;

	TimeDetail::TimePeriod getPeriod() const;

	const std::string & getChannelName() const;

	const std::string & getData() const;

	const std::vector<LineView> & getLines() const;

	std::size_t getNumberOfLines() const;

private:
	bool m_valid = false;
	const TimeDetail::TimePeriod m_period;
	ChannelName m_channel_name;
	std::string m_data;
	std::vector<LineView> m_lines;
};

#endif // !Log_HEADER
