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
#include <set>
#include <unordered_map>

//Boost
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/binary_object.hpp>

//local
#include "TimeDetail.hpp"

namespace Log
{
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

	class Log
	{
	public:
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

	namespace Impl
	{
		/*
		Log file format (64 bit):
		offset type size:		8 byte
		size type size:			8 byte
		timepoint element size: 16 byte
		view element size:		16 byte
		Header:
			0x0		channel_name_offset
			0x8		channel_name_size (in byte)
			0x10	username_data_offset
			0x18	username_data_size (in byte)
			0x20	message_data_offset
			0x28	message_data_size (in byte)
			0x30	timepoint_data_offset
			0x38	username_view_offset
			0x40	message_view_offset
			0x48	number_of_lines

			0x50	channel_name

					timepoint_data

					username_data

					message_data

					username_view

					message_view
		*/

		struct DataView
		{
			std::uint64_t ptr;
			std::uint64_t size;
		};

		struct LogFileHeader
		{
			std::uint64_t channel_name_offset = 0x50;
			std::uint64_t channel_name_size;
			std::uint64_t username_data_offset;
			std::uint64_t username_data_size;
			std::uint64_t message_data_offset;
			std::uint64_t message_data_size;
			std::uint64_t timepoint_data_offset;
			std::uint64_t username_view_offset;
			std::uint64_t message_view_offset;
			std::uint64_t number_of_lines;
		};
	}

	template <class OutStream>
	void serializeDefaultLogFormat(OutStream & out_stream, const Log & log)
	{
		using namespace Impl;

		std::unordered_map<std::string_view, std::vector<std::uint64_t>> name_map;
		std::vector<DataView> name_view(log.getLines().size());
		{
			{
				std::uint64_t i = 0;
				for (auto & name : log.getLines()) {
					auto it = name_map.find(name);
					if (it == name_map.end()) {
						name_map.emplace(name, std::vector<std::uint64>{ i });
					}
					else {
						it->second.push_back(i);
					}
					++i;
				}
			}
			for (auto & pair : name_map) {
				for (std::uint64_t i : pair.second) {
					name_view[i] = DataView{ i, pair.first.size() };
				}
			}
		}
		
		LogFileHeader header;
		//header.channel_name_offset
		header.channel_name_size = log.getChannelName().size();
		header.username_data_offset = //??????
		header.username_data_size = //??????


		header.timepoint_data_offset = header.channel_name_offset + header.channel_name_size;
		
		
		std::uintptr_t data_ptr = static_cast<std::uintptr_t>(log.getData().data());
		
		
		

	}

	template <class InStream>
	void deserializeDefaultLogFormat(InStream & in_stream)
	{
		using namespace Impl;
	}

}

#endif // !Log_HEADER
