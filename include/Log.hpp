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
#include <cstdint>

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
		inline bool isBigEndian()
		{
			union {
				std::uint32_t i;
				char c[4];
			} num = { 0x01020304 };
			return num.c[0] == 1;
		}

		template <typename ByteFieldType, typename OffsetType, typename SizeType, typename TimePeriodType>
		struct LogFileHeader
		{
			ByteFieldType format_mode;
			ByteFieldType endianness;
			OffsetType channel_name_offset;
			OffsetType username_data_offset;
			OffsetType message_data_offset;
			OffsetType timepoint_list_offset;
			OffsetType username_list_offset;
			OffsetType message_list_offset;
			SizeType channel_name_size;
			SizeType username_data_size;
			SizeType message_data_size;
			SizeType number_of_lines;
			TimePeriodType time_period;
		};

		template <typename SizeType, typename StringType>
		struct UsernameDataElement
		{
			SizeType size;
			StringType str;
		};

		template <typename OffsetType, typename SizeType>
		struct MessageListElement
		{
			OffsetType offset;
			SizeType size;
		};

		template <class OutStream, typename ByteFieldType, typename DataOffsetType, typename SizeType, typename FileOffsetType = std::uint64_t, typename TimePointType = TimeDetail::TimePoint, typename TimePeriodType = TimeDetail::TimePeriod>
		void serialize(OutStream & out_stream, const Log & log, ByteFieldType format_mode, ByteFieldType endianness)
		{
			using HeaderType = LogFileHeader<ByteFieldType, FileOffsetType, SizeType, TimePeriodType>;
			using UsernameDataElementType = UsernameDataElement<SizeType, std::string_view>;
			using MessageElementType = MessageListElement<DataOffsetType, SizeType>;

			auto write_type_func = [&](auto type) { out_stream.write(reinterpret_cast<char*>(type), sizeof(type)); };
			auto write_padding_func = [&](std::size_t amount) { for (std::size_t i = 0; i < amount; ++i) out_stream.put('\0'); };
			auto write_view_func = [&](std::string_view view) {out_stream.write(view.data(), view.size()); };

			HeaderType header;

			header.format_mode = format_mode;
			header.endianness = endianness;

			if (format_mode == 1) { //16-bit
				header.channel_name_offset = 0x58;
			}
			else if (format_mode == 2) { //32-bit
				header.channel_name_offset = 0x60;
			}
			else if (format_mode == 3) { //64-bit
				header.channel_name_offset = 0x70;
			}

			header.channel_name_size = static_cast<SizeType>(log.getChannelName().size());

			std::vector<UsernameDataElementType> username_data;
			std::vector<DataOffsetType> username_list(log.getLines().size());
			header.username_data_offset = header.channel_name_offset + header.channel_name_size;
			header.username_data_size = 0;
			{
				std::unordered_map<std::string_view, std::vector<SizeType>> name_map;
				{
					SizeType i = 0;
					for (auto & name : log.getLines()) {
						auto it = name_map.find(name);
						if (it == name_map.end()) {
							name_map.emplace(name, std::vector<SizeType>{ i });
						}
						else {
							it->second.push_back(i);
						}
						++i;
					}
				}
				for (auto & pair : name_map) {
					username_data.push_back(UsernameDataElementType{ pair.first.size(), pair.first });
					for (auto i : pair.second) {
						username_list[i] = header.username_data_size;
					}
					header.username_data_size += (sizeof(SizeType) + pair.first.size());
				}
			}
			
			std::vector<MessageElementType> message_list;
			header.message_data_offset = header.username_data_offset + header.username_data_size;
			header.message_data_size = 0;
			{
				message_list.reserve(log.getLines().size());
				for (auto & line : log.getLines()) {
					message_list.push_back(MessageElementType{ header.message_data_size, line.getMessageView().size() });
					header.message_data_size += line.getMessageView().size();
				}
			}
			
			header.number_of_lines = static_cast<SizeType>(log.getLines().size());
			header.timepoint_list_offset = header.message_data_offset + header.message_data_size;
			header.username_list_offset = header.timepoint_list_offset + sizeof(TimePointType) * header.number_of_lines;
			header.message_list_offset = header.username_list_offset + sizeof(DataOffsetType) * header.number_of_lines;
			header.time_period = log.getPeriod();

			write_type_func(header.format_mode);
			write_type_func(header.endianness);
			write_padding_func(6);
			write_type_func(header.channel_name_offset);
			write_type_func(header.username_data_offset);
			write_type_func(header.message_data_offset);
			write_type_func(header.timepoint_list_offset);
			write_type_func(header.username_list_offset);
			write_type_func(header.message_list_offset);
			write_type_func(header.channel_name_size);
			write_type_func(header.username_data_size);
			write_type_func(header.message_data_size);
			write_type_func(header.number_of_lines);

			write_view_func(log.getChannelName());
			for (auto & e : username_data) {
				write_type_func(e.size);
				write_view_func(e.str);
			}
			for (auto & line : log.getLines()) {
				write_view_func(line.getMessageView());
			}

			for (auto & e : log.getLines()) {
				write_type_func(e.getTime());
			}

			for (DataOffsetType e : username_list) {
				write_type_func(e);
			}

			for (auto & e : message_list) {
				write_type_func(e.offset);
				write_type_func(e.size);
			}

		}
	}

	template <class OutStream>
	void serializeDefaultLogFormat(OutStream & out_stream, const Log & log)
	{

	}

	template <class InStream>
	void deserializeDefaultLogFormat(InStream & in_stream)
	{
		using namespace Impl;
	}

}

#endif // !Log_HEADER
