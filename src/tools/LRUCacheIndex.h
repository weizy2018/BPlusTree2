/*
 * LRUCachIndex.h
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#ifndef TOOLS_LRUCACHEINDEX_H_
#define TOOLS_LRUCACHEINDEX_H_

#include "lru.h"
#include "../BPlusTree.h"

#define CACHE_SIZE 10

class LRUCacheIndex {
public:
	static LRUCacheIndex * getLruInst();
	static void releaseLruInst();
	LruCache<unsigned long int, TreeNode*> * getLruCache();
private:
	LRUCacheIndex();
	static LRUCacheIndex * lruInst;
	LruCache<unsigned long int, TreeNode*> * lruCache;
public:
	virtual ~LRUCacheIndex();
};

#endif /* TOOLS_LRUCACHEINDEX_H_ */
