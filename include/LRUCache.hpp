//LRUCache.hpp
#pragma once
#ifndef LRUCache_HEADER
#define LRUCache_HEADER

//C++
#include <list>
#include <unordered_map>
#include <optional>
#include <type_traits>
#include <algorithm>
#include <functional>
#include <cassert>

template <typename KeyType, typename ValueType, typename SizeType = std::size_t>
class LRUCache
{
public:
	static_assert(std::is_integral<SizeType>::value, "Invalid SizeType");

	using QueuePair = std::pair<KeyType, ValueType>;

	LRUCache() :
		m_size(0)
	{
	}

	LRUCache(SizeType size) :
		m_size(size)
	{
	}

	void put(KeyType && key, ValueType && value)
	{
		if (m_size != 0 && m_queue.size() == m_size) {
			assert(m_queue.size() <= m_size);
			popBack();
		}
		//if key exist, override data
		auto it = m_map.find(key);
		if (it == m_map.end()) {
			m_queue.emplace_front(key, std::forward<ValueType>(value));
			m_map.emplace(std::forward<KeyType>(key), m_queue.begin());
		}
		else {
			it->second->second = std::forward<ValueType>(value);
			if (it->second != m_queue.begin()) {
				moveFront(it);
			}
		}
	}

	//Returns iterator from m_queue.
	//If the iterator is not equal to m_queue.end() then the element is present.
	//The iterator can be invalidated if it is erased from a put.
	auto get(const KeyType & key)
	{
		auto it = m_map.find(key);
		if (it != m_map.end()) {
			if (it->second != m_queue.begin()) {
				moveFront(it);
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

	SizeType size() const
	{
		return size;
	}

	void resize(const SizeType size)
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
	std::list<QueuePair> m_queue;
	std::unordered_map<KeyType, typename std::list<QueuePair>::iterator> m_map;
	SizeType m_size; // m_size == 0 : no size limit, m_size > 0 : limited to size

	void moveFront(typename decltype(m_map)::iterator & it)
	{
		m_queue.push_front(std::move(*(it->second)));
		m_queue.erase(it->second);
		it->second = m_queue.begin();
	}

	void popBack()
	{
		auto map_it = m_map.find(m_queue.back().first);
		assert(map_it != m_map.end());
		m_map.erase(map_it);
		m_queue.pop_back();
	}
};



#endif // !LRUCache_HEADER
