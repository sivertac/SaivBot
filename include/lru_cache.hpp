//lru_cache.hpp
#pragma once
#ifndef lru_cache_HEADER
#define lru_cache_HEADER

//C++
#include <list>
#include <unordered_map>
#include <optional>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <cassert>

template <typename key_type, typename value_type, class hash = std::hash<key_type>, typename size_type = std::size_t>
class lru_cache
{
public:
	static_assert(std::is_integral<size_type>::value, "Invalid size_type");

	using queue_pair = std::pair<key_type, value_type>;

	lru_cache() :
		m_size(0)
	{
	}

	lru_cache(size_type size) :
		m_size(size)
	{
	}

	void put(key_type && key, value_type && value)
	{
		if (m_size != 0 && m_queue.size() == m_size) {
			assert(m_queue.size() <= m_size);
			pop_back();
		}
		//if key exist, override data
		auto it = m_map.find(key);
		if (it == m_map.end()) {
			m_queue.emplace_front(key, std::forward<value_type>(value));
			m_map.emplace(std::forward<key_type>(key), m_queue.begin());
		}
		else {
			it->second->second = std::forward<value_type>(value);
			if (it->second != m_queue.begin()) {
				move_front(it);
			}
		}
	}

	//Returns iterator from m_queue.
	//If the iterator is not equal to m_queue.end() then the element is present.
	//The iterator can be invalidated if it is erased from a put.
	auto get(const key_type & key)
	{
		auto it = m_map.find(key);
		if (it != m_map.end()) {
			if (it->second != m_queue.begin()) {
				move_front(it);
			}
			return (it->second);
		}
		else {
			return m_queue.end();
		}
	}

	auto begin() const
	{
		return m_queue.begin();
	}

	auto end() const
	{
		return m_queue.end();
	}

	const auto & get_queue() const
	{
		return m_queue;
	}

	const auto & get_map() const
	{
		return m_map;
	}

	size_type size() const
	{
		return size;
	}

	void resize(const size_type size)
	{
		if (size != 0 && m_queue.size() > size) {
			for (auto queue_it = std::next(m_queue.begin(), size); queue_it != m_queue.end();) {
				auto map_it = m_map.find(queue_it->first);
				assert(map_it != m_map.end());
				m_map.erase(map_it);
				queue_it = m_queue.erase(queue_it);
			}
		}
		m_size = size;
	}

	void remove_size_limit()
	{
		resize(0);
	}

private:
	std::list<queue_pair> m_queue;
	std::unordered_map<key_type, typename std::list<queue_pair>::iterator, hash> m_map;
	size_type m_size; // m_size == 0 : no size limit, m_size > 0 : limited to size

	void move_front(typename decltype(m_map)::iterator & it)
	{
		m_queue.push_front(std::move(*(it->second)));
		m_queue.erase(it->second);
		it->second = m_queue.begin();
	}

	void pop_back()
	{
		auto map_it = m_map.find(m_queue.back().first);
		assert(map_it != m_map.end());
		m_map.erase(map_it);
		m_queue.pop_back();
	}
};



#endif // !lru_cache_HEADER
