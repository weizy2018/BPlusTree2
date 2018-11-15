/*
 * LRUCachIndex.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#include "LRUCacheIndex.h"

template<typename key, typename value>
LRUCacheIndex<key, value> * LRUCacheIndex<key, value>::lruInst = nullptr;


template<typename key, typename value>
LRUCacheIndex<key, value>::LRUCacheIndex() {

	lruCache = new LruCache<unsigned long int, TreeNode<key, value>*>(CACHE_SIZE);

}

template<typename key, typename value>
LRUCacheIndex<key, value>::~LRUCacheIndex() {
	delete lruCache;
}

template<typename key, typename value>
LRUCacheIndex<key, value> * LRUCacheIndex<key, value>::getLruInst() {
	if (lruInst == nullptr) {
		lruInst = new LRUCacheIndex<key, value>();
	}
	return lruInst;
}

template<typename key, typename value>
void LRUCacheIndex<key, value>::releaseLruInst() {
	delete lruInst;
}

