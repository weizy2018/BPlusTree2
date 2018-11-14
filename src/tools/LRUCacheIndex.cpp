/*
 * LRUCachIndex.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#include "LRUCacheIndex.h"

LRUCacheIndex * LRUCacheIndex::lruInst = nullptr;

LRUCacheIndex::LRUCacheIndex() {

	lruCache = new LruCache<unsigned long int, TreeNode*>(CACHE_SIZE);

}

LRUCacheIndex::~LRUCacheIndex() {
	delete lruCache;
}

LRUCacheIndex * LRUCacheIndex::getLruInst() {
	if (lruInst == nullptr) {
		lruInst = new LRUCacheIndex();
	}
	return lruInst;
}
void LRUCacheIndex::releaseLruInst() {
	delete lruInst;
}

