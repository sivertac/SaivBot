//OptionParser.hpp
//Author: Sivert Andresen Cubedo

#pragma once
#ifndef OptionParser_HEADER
#define OptionParser_HEADER

//C++
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <istream>
#include <tuple>
#include <functional>
#include <array>
#include <utility>

class Option;

class Result
{
public:
	Result(const Option & option, std::vector<std::string> && args) :
		m_option(option),
		m_args(std::move(args))
	{
	}
	const Option & getOption() const
	{
		return m_option;
	}
	const std::vector<std::string> & getArgs() const
	{
		return m_args;
	}
	const std::string & operator[](std::size_t i) const
	{
		return m_args[i];
	}
private:
	const Option & m_option;
	std::vector<std::string> m_args;
};

class ResultSet
{
public:
	/*
	*/
	ResultSet(std::vector<Result> && vec) :
		m_container(std::move(vec))
	{
	}
	
	/*
	Search for option.
	*/
	std::optional<std::reference_wrapper<const Result>> find(const Option & opt) const
	{
		auto it = std::find_if(m_container.begin(), m_container.end(), [&](auto & res) {return &res.getOption() == &opt; });
		if (it != m_container.end()) {
			return *it;
		}
		else {
			return std::nullopt;
		}
	}

	/*
	Get m_container.
	*/
	const std::vector<Result> & getVec() const
	{
		return m_container;
	}

private:
	std::vector<Result> m_container;
};

class Option
{
public:
	Option(const std::vector<std::string> & identifiers, int args = 0) :
		m_identifiers(identifiers),
		m_args(args)
	{
	}

	Option(const std::string & identifier, int args = 0) :
		Option(std::vector<std::string>{identifier}, args)
	{
	}
	
	bool checkIdentifier(const std::string & str) const
	{
		auto it = std::find_if(m_identifiers.begin(), m_identifiers.end(), [&](auto & id) {return str == id; });
		if (it == m_identifiers.end()) {
			return false;
		}
		else {
			return true;
		}
	}

	std::optional<Result> parse(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end) const
	{
		int vec_size = static_cast<int>(std::distance(begin, end));
		if (vec_size < m_args + 1) {
			return std::nullopt;
		}
		if (m_args == 0) {
			return Result(*this, std::vector<std::string>{});
		}
		else {
			return Result(*this, std::vector<std::string>{begin + 1, begin + m_args + 1});
		}
	}

	std::optional<Result> parse(const std::vector<std::string> & in_vec) const
	{
		return parse(in_vec.begin(), in_vec.end());
	}

	int getSize() const
	{
		return m_args + 1;
	}

	auto & getIdentifiers() const
	{
		return m_identifiers;
	}

private:
	const std::vector<std::string> m_identifiers;
	int m_args;
	std::string m_help;
};

template <typename T1 = void, typename T2 = void, typename ... Ts>
constexpr bool allSame()
{
	if (std::is_void<T2>::value) {
		return true;
	}
	else if (std::is_same<T1, T2>::value) {
		return allSame<T2, Ts...>();
	}
	else {
		return false;
	}
}

template <class ... Os>
class OptionParser
{
public:
	static_assert(allSame<Option, Os...>(), "All parameters must be of type Option");

	using OptionContainer = std::array<Option, sizeof...(Os)>;

	OptionParser(Os ... os) :
		m_options{ os... }
	{
	}

	/*
	*/
	ResultSet parse(std::vector<std::string>::const_iterator begin, std::vector<std::string>::const_iterator end) const
	{
		std::vector<Result> result_vec;

		if (std::distance(begin, end) == 0) return ResultSet(std::move(result_vec));

		while (begin != end) {
			const std::string & str = *begin;
			auto option_it = std::find_if(m_options.begin(), m_options.end(), [&](auto & opt) {return opt.checkIdentifier(str); });
			if (option_it == m_options.end()) {
				//option does not exist
				++begin;
			}
			else {
				//parse option
				if (auto result = option_it->parse(begin, end)) {
					result_vec.push_back(*result);
					begin += result_vec.back().getOption().getSize();
				}
				else {
					++begin;
				}
			}
		}
		return ResultSet(std::move(result_vec));
	}

	/*
	*/
	ResultSet parse(const std::vector<std::string> & input_vec) const
	{
		return parse(input_vec.begin(), input_vec.end());
	}

	/*
	*/
	ResultSet parse(const std::string & line) const
	{
		std::istringstream iss(line);
		std::vector<std::string> vec{ std::istream_iterator<std::string>{iss}, std::istream_iterator<std::string>{} };
		return parse(vec);
	}

	/*
	*/
	ResultSet parse(std::vector<std::string_view>::const_iterator begin, std::vector<std::string_view>::const_iterator end) const
	{
		std::vector<std::string> vec{ begin, end };
		return parse(vec);
	}

	/*
	Get optons.
	*/
	const OptionContainer & getOptions() const
	{
		return m_options;
	}

	/*
	Get option by index.
	*/
	template <std::size_t I>
	constexpr const Option & get() const
	{
		return std::get<I>(m_options);
	}
private:
	std::array<Option, sizeof...(Os)> m_options;
};

#endif // !OptionParser_HEADER
