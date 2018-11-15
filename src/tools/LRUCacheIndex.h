/*
 * LRUCachIndex.h
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#ifndef TOOLS_LRUCACHEINDEX_H_
#define TOOLS_LRUCACHEINDEX_H_

#include "lru.h"
//#include "../BPlusTree.h"

#define CACHE_SIZE 10

template<typename key, typename value>
class BPlusTree;

template<typename key, typename value>
class TreeNode;

template<typename key, typename value>
class LRUCacheIndex {
public:
	static LRUCacheIndex * getLruInst();
	static void releaseLruInst();

	LruCache<unsigned long int, TreeNode<key, value>*> * getLruCache();
private:
	LRUCacheIndex();					//单例模式
	static LRUCacheIndex * lruInst;

	LruCache<unsigned long int, TreeNode<key, value>*> * lruCache;
public:
	virtual ~LRUCacheIndex();
};


#endif /* TOOLS_LRUCACHEINDEX_H_ */
