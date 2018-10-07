//TimeDetail.hpp
#pragma once
#ifndef TimeDetail_HEADER
#define TimeDetail_HEADER

//C++
#include <iostream>
#include <string>
#include <memory>
#include <array>
#include <functional>
#include <optional>
#include <unordered_map>
#include <mutex>
#include <chrono>

//Date
#include <date/date.h>

namespace TimeDetail
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	inline std::string monthToString(date::month month)
	{
		std::stringstream ss;
		ss << date::format("%B", month);
		return ss.str();
	}

	inline std::optional<TimePoint> parseTimeString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		TimePoint point;
		ss >> date::parse("%F %T", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::year_month_day> parseDateString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::year_month_day point;
		ss >> date::parse("%F", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::month> parseMonthString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::month point;
		ss >> date::parse("%B", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<date::year> parseYearString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		date::year point;
		ss >> date::parse("%g", point);
		if (ss.fail()) {
			return std::nullopt;
		}
		else {
			return point;
		}
	}

	inline std::optional<TimePoint> parseTimePointString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		std::chrono::system_clock::time_point point;
		if (!(ss >> date::parse("%Y-%m-%d-%H-%M-%S", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d-%H-%M", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d-%H", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		if (!(ss >> date::parse("%Y-%m-%d", point)).fail()) {
			return point;
		}
		ss = std::stringstream();
		ss << view;
		date::year_month year_month;
		if (!(ss >> date::parse("%Y-%m", year_month)).fail()) {
			return date::sys_days(year_month / 1);
		}
		ss = std::stringstream();
		ss << view;
		date::year year;
		if (!(ss >> date::parse("%Y", year)).fail()) {
			return date::sys_days(year / 1 / 1);
		}
		return std::nullopt;
	}

	class TimePeriod
	{
	public:
		TimePeriod() = default;
		TimePeriod(TimePoint begin, TimePoint end) :
			m_begin(begin),
			m_end(end)
		{
			assert(begin <= end);
		}

		bool isInside(TimePoint point) const
		{
			if (point >= m_begin && point < m_end) {
				return true;
			}
			else {
				return false;
			}
		}

		TimePoint begin() const
		{
			return m_begin;
		}

		TimePoint end() const
		{
			return m_end;
		}
	private:
		TimePoint m_begin;
		TimePoint m_end;
	};

	inline std::optional<TimePeriod> parseTimePeriod(const std::string_view & begin, const std::string_view & end)
	{
		TimePoint begin_time;
		TimePoint end_time;
		if (auto r = parseTimePointString(begin)) {
			begin_time = *r;
		}
		else {
			return std::nullopt;
		}
		if (auto r = parseTimePointString(end)) {
			end_time = *r;
		}
		else {
			return std::nullopt;
		}
		if (begin_time > end_time) {
			return std::nullopt;
		}
		return TimePeriod(begin_time, end_time);
	}

	inline TimePeriod createYearMonthPeriod(const date::year_month & ym)
	{
		auto date_begin = date::year_month_day(ym.year(), ym.month(), date::day(1));
		auto date_end = date_begin + date::months(1);
		TimeDetail::TimePoint time_begin = date::sys_days(date_begin);
		TimeDetail::TimePoint time_end = date::sys_days(date_end);
		return TimeDetail::TimePeriod(time_begin, time_end);
	}
};

#endif // !TimeDetail_HEADER
