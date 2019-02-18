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
			TimeDetail::TimePoint time,
			std::string_view name_view,
			std::string_view message_view
		);

		TimeDetail::TimePoint getTime() const;

		std::string_view getNameView() const;

		std::string_view getMessageView() const;

		static LineView copyView(const char* old_offset, const char* new_offset, LineView source);

	private:
		TimeDetail::TimePoint m_time;
		std::string_view m_name_view;
		std::string_view m_message_view;
	};

	class Line
	{
	public:
		Line(const LineView & line_view);

		Line(const Line & source);
		
		Line & operator=(const Line & source)
		{
			copyLineImpl(source);
			return *this;
		}

		Line(Line && source);

		Line & operator=(Line && source)
		{
			moveLineImpl(std::move(source));
			return *this;
		}

		std::string & getData();

		TimeDetail::TimePoint getTime() const;
		
		std::string_view getNameView() const;
		
		std::string_view getMessageView() const;
	private:
		std::string m_data;
		TimeDetail::TimePoint m_time;
		std::string_view m_name_view;
		std::string_view m_message_view;

		void copyLineViewImpl(const LineView & line_view);

		void copyLineImpl(const Line & source);

		void moveLineImpl(Line && source);
	};

	using ParserFunc = std::function<bool(const std::string&, std::vector<std::string_view> & names, std::vector<LineView> & lines)>;
	using ChannelName = std::string;

	class Log
	{
	public:
		Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, std::vector<std::string_view> && names, std::vector<LineView> && lines);
		
		Log(TimeDetail::TimePeriod && period, ChannelName && channel_name, std::string && data, ParserFunc parser);

		Log(const Log & source) = delete;
		Log & operator=(const Log & source) = delete;

		Log(Log && source)
		{
			moveImpl(std::move(source));
		}
		Log & operator=(Log && source)
		{
			moveImpl(std::move(source));
			return *this;
		}

		bool isValid() const;

		TimeDetail::TimePeriod getPeriod() const;

		const std::string & getChannelName() const;

		const std::string & getData() const;

		const std::vector<std::string_view> & getNames() const;

		const std::vector<LineView> & getLines() const;

	private:
		bool m_valid = false;
		TimeDetail::TimePeriod m_period;
		ChannelName m_channel_name;
		std::string m_data;
		std::vector<std::string_view> m_names; //sorted set of names (no duplicates)
		std::vector<LineView> m_lines;

		void moveImpl(Log && source);
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

		template <typename ByteFieldType, typename FileOffsetType, typename SizeType, typename TimePeriodType>
		struct LogFileHeader
		{
			ByteFieldType format_mode;
			ByteFieldType endianness;
			FileOffsetType channel_name_offset;
			FileOffsetType username_data_offset;
			FileOffsetType username_set_offset;
			FileOffsetType message_data_offset;
			FileOffsetType timepoint_list_offset;
			FileOffsetType username_list_offset;
			FileOffsetType message_list_offset;
			SizeType channel_name_size;
			SizeType username_data_size;
			SizeType username_set_size;
			SizeType message_data_size;
			SizeType number_of_lines;
			TimePeriodType time_period;
		};

		template <typename SizeType, typename StringType>
		struct StringDataElement
		{
			SizeType size;
			StringType str;
		};

		template <class OutStream, typename ByteFieldType, typename DataOffsetType, typename SizeType, typename FileOffsetType = std::uint64_t, typename TimePointType = TimeDetail::TimePoint, typename TimePeriodType = TimeDetail::TimePeriod>
		void serialize(OutStream & out_stream, const Log & log, ByteFieldType format_mode, ByteFieldType endianness)
		{
			using HeaderType = LogFileHeader<ByteFieldType, FileOffsetType, SizeType, TimePeriodType>;
			using StringDataElementType = StringDataElement<SizeType, std::string_view>;
			
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

			std::vector<DataOffsetType> username_list();
			header.username_data_offset = header.channel_name_offset + header.channel_name_size;
			header.username_data_size = 0;
			{
				//THIS IS WRONG

				username_list.reserve(log.getLines().size());
				for (auto & line : log.getLines()) {
					username_list.push_back(header.username_data_size);
					header.username_data_size += sizeof(SizeType) + line.getNameView().size();
				}
			}
			
			std::vector<DataOffsetType> message_list;
			header.message_data_offset = header.username_data_offset + header.username_data_size;
			header.message_data_size = 0;
			{
				message_list.reserve(log.getLines().size());
				for (auto & line : log.getLines()) {
					message_list.push_back(header.message_data_size);
					header.message_data_size += sizeof(SizeType) + line.getMessageView().size();
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

			/*
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
			*/

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
