/*
 * lru.h
 *
 *  Created on: Nov 7, 2018
 *      Author: weizy
 */

#ifndef TOOLS_HEAD_LRU_H_
#define TOOLS_HEAD_LRU_H_

#include <unordered_map>
#include <list>
#include <cstddef>
#include <stdexcept>

#include <iostream>
using namespace std;

template<typename key_t, typename value_t>
class LruCache {
public:
	typedef typename std::pair<key_t, value_t> key_value_pair_t;
	typedef typename std::list<key_value_pair_t>::iterator list_iterator_t;

	LruCache(size_t max_size) :
		_max_size(max_size) {
	}
	~LruCache() {
		cout << "~LruCache()" << endl;
		while(!_cache_items_list.empty()) {
			key_value_pair_t fro = _cache_items_list.front();
			_cache_items_list.pop_front();
			delete fro.second;
		}
	}

	value_t put(const key_t& key, const value_t& value) {
		value_t v = 0;
		auto it = _cache_items_map.find(key);
		_cache_items_list.push_front(key_value_pair_t(key, value));
		if (it != _cache_items_map.end()) {
			_cache_items_list.erase(it->second);
			_cache_items_map.erase(it);
		}
		_cache_items_map[key] = _cache_items_list.begin();

		if (_cache_items_map.size() > _max_size) {
			auto last = _cache_items_list.end();
			last--;

			cout << "key = " << last->first << "  value = " << last->second << endl;
			v = last->second;
			_cache_items_map.erase(last->first);
			_cache_items_list.pop_back();
		}
		return v;
	}

	const value_t& get(const key_t& key) {
		auto it = _cache_items_map.find(key);
		if (it == _cache_items_map.end()) {
			throw std::range_error("There is no such key in cache");
		} else {
			_cache_items_list.splice(_cache_items_list.begin(), _cache_items_list, it->second);
			return it->second->second;
		}
	}

	bool exists(const key_t& key) const {
		return _cache_items_map.find(key) != _cache_items_map.end();
	}

	size_t size() const {
		return _cache_items_map.size();
	}

private:
	std::list<key_value_pair_t> _cache_items_list;
	std::unordered_map<key_t, list_iterator_t> _cache_items_map;
	size_t _max_size;
};




#endif /* TOOLS_HEAD_LRU_H_ */
