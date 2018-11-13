/*
 * BPlusTree.cpp
 *
 *  Created on: Nov 13, 2018
 *      Author: weizy
 */

#include "BPlusTree.h"

template<typename key, typename value>
BPlusTree<key, value>::BPlusTree(const char * indexFileName, int keyLen, int valueLen, bool create) {
	this->indexFileName = indexFileName;
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	head = (char*)malloc(sizeof(totalBlock) + sizeof(root));

	if (create) {
		createIndex();
	} else {
		init();
	}

}
template<typename key, typename value>
BPlusTree<key, value>::~BPlusTree() {
	free(head);
}

template<typename key, typename value>
void BPlusTree<key, value>::init() {
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		printf("FileNotFoundException\n");
		exit(0);
	}
	fread(head, sizeof(totalBlock) + sizeof(root), 1, indexFile);
	fclose(indexFile);

	char * h = head;
	memcpy((char*) &totalBlock, h, sizeof(totalBlock));
	memcpy((char*) &root, h, sizeof(root));
}

template<typename key, typename value>
void BPlusTree<key, value>::createIndex() {
	//添加头信息	添加一个空块 root
}

template<typename key, typename value>
void BPlusTree<key, value>::put(key k, value v) {


}

template<typename key, typename value>
value BPlusTree<key, value>::get(key k) {

}



//------------------------------------
//TreeNode
//------------------------------------
template<typename key, typename value>
TreeNode<key,value>::TreeNode(int keyLen, int valueLen, unsigned long int self, unsigned long int parent,int type, const char * indexFileName) {
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	this->self = self;
	this->parent = parent;
	this->type = type;
	this->count = 0;
	this->next = -1;
	this->indexFileName = indexFileName;
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
}
template<typename key, typename value>
TreeNode<key,value>::TreeNode(const char * block, int keyLen, int valueLen, const char * indexFileName) {
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	this->indexFileName = indexFileName;
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
	parsed(block);
}

template<typename key, typename value>
TreeNode<key,value>::~TreeNode() {
	free(data);
}

template<typename key, typename value>
void TreeNode<key,value>::addData(key k, value v) {
	char * d = data;
	unsigned int len = valueLen + keyLen;
	d = d + len*count;
	memcpy(d, (char*)&v, valueLen);
	memcpy(d, (char*)&k, keyLen);
	count++;
}

template<typename key, typename value>
void TreeNode<key,value>::parsed(const char * block) {
	const char * b;
	//self
	memcpy((char*)&self, b, sizeof(self));
	b += sizeof(self);

	//type
	memcpy((char*)&type, b, sizeof(type));
	b += sizeof(type);

	//count
	memcpy((char*)&count, b, sizeof(count));
	b += sizeof(count);

	//parent
	memcpy((char*)&parent, b, sizeof(parent));
	b += sizeof(parent);

	//next
	memcpy((char*)&next, b, sizeof(next));
	b += sizeof(next);

	//data
	memcpy(data, b, TREE_NODE_DATA_SIZE);
}

template<typename key, typename value>
void TreeNode<key,value>::writeBack() {
	char * block = (char*)malloc(BLOCK_SIZE);
	char * b = block;
	//self
	memcpy(b, (char*)&self, sizeof(self));
	b += sizeof(self);

	//type
	memcpy(b, (char*)&type, sizeof(type));
	b += sizeof(type);

	//count
	memcpy(b, (char*)&count, sizeof(count));
	b += sizeof(count);

	//parent
	memcpy(b, (char*)&parent, sizeof(parent));
	b += sizeof(parent);

	//next
	memcpy(b, (char*)&next, sizeof(next));
	b += sizeof(next);

	memcpy(b, data, TREE_NODE_DATA_SIZE);

	//write to disk
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb+")) == NULL) {
		printf("FileNotFoundException");
		exit(0);
	}
	fseek(indexFile, self*BLOCK_SIZE + OFFSER_LENGTH, SEEK_SET);
	fwrite(block, BLOCK_SIZE, 1, indexFile);

	fclose(indexFile);
}

template<typename key, typename value>
unsigned long int TreeNode<key,value>::getSelf() {
	return self;
}
template<typename key, typename value>
unsigned long int TreeNode<key,value>::getParent() {
	return parent;
}
template<typename key, typename value>
unsigned long int TreeNode<key,value>::getNext() {
	return next;
}
template<typename key, typename value>
int TreeNode<key,value>::getType() {
	return type;
}

template<typename key, typename value>
int TreeNode<key,value>::getCount() {
	return count;
}
template<typename key, typename value>
key TreeNode<key,value>::getKey(int index) {
	key k;
	const char * d = data;
	unsigned int len = keyLen + valueLen;
	d = d + len*index;
	d += valueLen;
	memcpy((char*)&k, d, keyLen);
}

template<typename key, typename value>
value TreeNode<key,value>::getValue(int index) {
	value v;
	const char * d = data;
	unsigned int len = keyLen + valueLen;
	d = d + len*index;
	memcpy((char*)&v, d, valueLen);
	return v;
}
















