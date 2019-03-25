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

//Boost
#include "boost/asio.hpp"

template <class Key, class T, class Hash = std::hash<Key>, class SizeType = std::size_t>
class lru_cache
{
public:
	static_assert(std::is_integral<SizeType>::value, "Invalid size_type");

	using key_type = Key;
	using value_type = T;
	using hasher = Hash;
	using size_type = SizeType;
	using queue_pair = std::pair<key_type, value_type>;

	lru_cache() :
		m_capacity(0)
	{
	}

	lru_cache(size_type capacity) :
		m_capacity(capacity)
	{
	}

	template <typename K, typename V>
	void put(K && key, V && value)
	{
		if (m_capacity != 0 && m_queue.size() == m_capacity) {
			assert(m_queue.size() <= m_capacity);
			pop_back();
		}
		//if key exist, override data
		auto it = m_map.find(key);
		if (it == m_map.end()) {
			m_queue.emplace_front(key, std::forward<V>(value));
			m_map.emplace(std::forward<K>(key), m_queue.begin());
		}
		else {
			it->second->second = std::forward<V>(value);
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

	auto begin() const noexcept
	{
		return m_queue.begin();
	}

	auto end() const noexcept
	{
		return m_queue.end();
	}

	const auto & get_queue() const noexcept
	{
		return m_queue;
	}

	const auto & get_map() const noexcept
	{
		return m_map;
	}

	//get number of elements in cache
	size_type size() const noexcept
	{
		return m_queue.size();
	}

	//get capacity of cache, if 0 then capacity is infinite
	size_type capacity() const noexcept
	{
		return m_capacity;
	}

	//set new size, will not add any elements
	void resize(const size_type size)
	{
		if (size != 0 && this->size() > size) {
			for (auto queue_it = std::next(m_queue.begin(), size); queue_it != m_queue.end();) {
				auto map_it = m_map.find(queue_it->first);
				assert(map_it != m_map.end());
				m_map.erase(map_it);
				queue_it = m_queue.erase(queue_it);
			}
		}
	}

	//set capacity, will evict elements if they are above the new capacity
	void reserve(const size_type capacity)
	{
		if (capacity < this->size())  {
			this->resize(capacity);
			m_capacity = capacity;
		}
		else {
			m_capacity = capacity;
		}
	}

	void remove_size_limit()
	{
		this->reserve(0);
	}

private:
	std::list<queue_pair> m_queue;
	std::unordered_map<key_type, typename std::list<queue_pair>::iterator, hasher> m_map;
	size_type m_capacity = 0; // if 0 then infinite

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
