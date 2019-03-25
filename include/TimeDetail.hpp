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

//json
#include <nlohmann/json.hpp>

//Boost
#include <boost/functional/hash.hpp>

namespace TimeDetail
{
	using TimePoint = std::chrono::time_point<std::chrono::system_clock>;

	inline std::string monthToString(date::month month)
	{
		std::stringstream ss;
		ss << date::format("%B", month);
		return ss.str();
	}

	inline std::optional<TimePoint> parseGempirTimeString(const std::string_view & view)
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

	inline std::optional<TimePoint> parseOverrustleTimeString(const std::string_view & view)
	{
		std::stringstream ss;
		ss << view;
		TimePoint point;
		ss >> date::parse("%F %T %Z", point);
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
		TimePeriod(TimePoint begin, TimePoint end) noexcept :
			m_begin(begin),
			m_end(end)
		{
			assert(begin <= end);
		}

		bool isInside(TimePoint point) const noexcept
		{
			if (point >= m_begin && point < m_end) {
				return true;
			}
			else {
				return false;
			}
		}

		TimePoint begin() const noexcept
		{
			return m_begin;
		}

		TimePoint end() const noexcept
		{
			return m_end;
		}

		bool isEqual(const TimePeriod & other) const noexcept
		{
			return (m_begin == other.m_begin && m_end == other.m_end);
		}

		friend bool operator==(const TimePeriod & lhs, const TimePeriod & rhs) noexcept
		{
			return lhs.isEqual(rhs);
		}

		friend bool operator!=(const TimePeriod & lhs, const TimePeriod & rhs) noexcept
		{
			return !(lhs == rhs);
		}

		struct hash
		{
			std::size_t operator()(const TimePeriod & p) const noexcept
			{
				std::size_t seed = 0;
				boost::hash_combine(seed, p.m_begin.time_since_epoch().count());
				boost::hash_combine(seed, p.m_end.time_since_epoch().count());
				return seed;
			}
		};

		friend void to_json(nlohmann::json & j, const TimePeriod & p)
		{
			j = nlohmann::json{
				{"begin", p.m_begin.time_since_epoch().count()},
				{"end", p.m_end.time_since_epoch().count()}
			};
		}

		friend void from_json(const nlohmann::json& j, TimePeriod & p)
		{
			TimePoint::rep rep;
			j.at("begin").get_to(rep);
			p.m_begin = TimePoint(std::chrono::system_clock::duration(rep));
			j.at("end").get_to(rep);
			p.m_end = TimePoint(std::chrono::system_clock::duration(rep));
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

	inline TimePeriod createYearMonthDayPeriod(const date::year_month_day & date)
	{
		auto day = date::sys_days{ date };
		TimeDetail::TimePoint begin = day;
		TimeDetail::TimePoint end = day + date::days(1);
		return TimeDetail::TimePeriod(begin, end);
	}

	inline std::vector<date::year_month> period_to_year_months(const TimePeriod & period)
	{
		std::vector<date::year_month> year_months;
		date::year_month_day ymd = date::floor<date::days>(period.begin());
		date::year_month begin_ym(ymd.year(), ymd.month());
		ymd = date::floor<date::days>(period.end());
		date::year_month end_ym(ymd.year(), ymd.month());
		end_ym += date::months(1);
		while (begin_ym < end_ym) {
			year_months.push_back(begin_ym);
			begin_ym += date::months(1);
		}
		return year_months;
	}

	inline std::vector<date::year_month_day> period_to_dates(const TimePeriod & period)
	{
		std::vector<date::year_month_day> dates;
		date::year_month_day begin = date::floor<date::days>(period.begin());
		date::year_month_day end = date::floor<date::days>(period.end());
		end = date::sys_days{ end } +date::days(1);
		while (begin < end) {
			dates.push_back(begin);
			begin = date::sys_days{ begin } +date::days(1);
		}
		return dates;
	}

};

#endif // !TimeDetail_HEADER
