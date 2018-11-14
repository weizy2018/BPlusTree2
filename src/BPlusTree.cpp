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
	treeNodeMaxSize = (TREE_NODE_DATA_SIZE - valueLen)/(keyLen + valueLen);

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


	char * h = head;
	memcpy((char*) &totalBlock, h, sizeof(totalBlock));
	h += sizeof(totalBlock);
	//根节点的位置
	memcpy((char*) &root, h, sizeof(root));
	fseek(indexFile, root*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
	char * block = (char*)malloc(BLOCK_SIZE);
	rootNode = new TreeNode<key, value>(block, keyLen, valueLen, indexFileName);

	fclose(indexFile);
}

template<typename key, typename value>
void BPlusTree<key, value>::createIndex() {
	//添加头信息	添加一个空块 root
	totalBlock = 1;
	root = OFFSET_LENGTH;

	//int keyLen, int valueLen, unsigned long int self, unsigned long int parent,int type, const char * indexFileNam
	//初始化是根节点为叶节点（type=1)
	rootNode = new TreeNode<key, value>(keyLen, valueLen, 0, -1, 1, indexFileName);

	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "wb")) == NULL) {
		printf("can't open the file\n");
		exit(0);
	}
	char * h = head;
	memcpy(h, (char*)&totalBlock, sizeof(totalBlock));
	h += sizeof(totalBlock);
	memcpy(h, (char*)&root, sizeof(root));
	fwrite(head, sizeof(totalBlock) + sizeof(root), 1, indexFile);
	fclose(indexFile);

	rootNode->writeBack();

}

template<typename key, typename value>
void BPlusTree<key, value>::put(key k, value v) {
	//找到应该包含k值的叶结点
	TreeNode<key, value> * leafNode = getLeafNode(k);
	//往叶结点中插入数据k，v
	leafNode->addData(k, v);	//已经实现
	if (leafNode->getCount() >= treeNodeMaxSize) {
		//分裂
	}

	//分裂的时候考虑TreeNode中的change,新建的结点中change应该置为真


	//判断插入后是否满了


}

template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getLeafNode(key k) {
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		throw FileNotFoundException(indexFileName);
	}
	TreeNode<key, value> * node = rootNode;
	TreeNode<key, value> * childNode = nullptr;
	while (node->getType() != 1) {
		unsigned long int nextAddr = node->getNextChild(k);
		try {	//先尝试从缓冲区获取
			childNode = LRUCacheIndex::getLruInst()->getLruCache()->get(nextAddr);

		} catch (exception & e) {	//缓冲区没找到
			char * child = (char*) malloc(BLOCK_SIZE);
			fread(child, BLOCK_SIZE, 1, indexFile);
			childNode = new TreeNode<key, value>(child, keyLen, valueLen,indexFileName);
			free(child);
			//放入LRU缓冲区
			TreeNode<key, value> * t = LRUCacheIndex::getLruInst()->getLruCache()->put(nextAddr, childNode);
			if (t) {
				delete t;
			}
		}
		node = childNode;
	}
	fclose(indexFile);
	return node;
}
/*
 * 查找关键词k在文件中的块号
 */
template<typename key, typename value>
value BPlusTree<key, value>::get(key k) {
	//rootNode 为叶节点 (即索引中只有一个节点，直接遍历该叶节点即可）
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		throw FileNotFoundException(indexFileName);
	}
	TreeNode<key, value> * node = rootNode;
	TreeNode<key, value> * childNode = nullptr;
	//查找关键词在那个子节点
	while (node->getType() != 1) {
		unsigned long int nextAddr = node->getNextChild(k);

		try {	//先尝试从缓冲区获取
			childNode = LRUCacheIndex::getLruInst()->getLruCache()->get(nextAddr);
		} catch (exception & e) {
			char * child = (char*)malloc(BLOCK_SIZE);
			fread(child, BLOCK_SIZE, 1, indexFile);
			childNode = new TreeNode<key, value>(child, keyLen, valueLen, indexFileName);
			free(child);
			//放入LRU缓冲区
			TreeNode<key, value> * t = LRUCacheIndex::getLruInst()->getLruCache()->put(nextAddr, childNode);
			if (t) {
				delete t;
			}
		}
		node = childNode;
	}
	fclose(indexFile);
	//查找关键词k是否在子节点中
	int index = node->binarySearch(k);
	//index == -1表示没找到，否则index表示索引值k的下标
	if (index==-1) {
		throw KeyNotFoundException(k);
	} else {
		return node->getValue(index);	//返回k对应的value，即为关键词对应关系表中的第几块
	}

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
//	this->next = -1;
	this->indexFileName = indexFileName;
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
	change = false;
}
template<typename key, typename value>
TreeNode<key,value>::TreeNode(const char * block, int keyLen, int valueLen, const char * indexFileName) {
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	this->indexFileName = indexFileName;
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
	change = false;
	parsed(block);
}

template<typename key, typename value>
TreeNode<key,value>::~TreeNode() {
	if (change) {
		writeBack();
	}
	free(data);
}

//插入后还应该保持有序
template<typename key, typename value>
void TreeNode<key, value>::addData(key k, value v) {

	char * d = data;
	if (count == 0) {	//直接添加
		memcpy(d, (char*) &v, valueLen);
		d += valueLen;
		memcpy(d, (char*) &k, keyLen);
	} else {
		unsigned int len = valueLen + keyLen;
		int n = 0;
		while (n < count && getKey(n) < k) {
			n++;
		}
		//插入位置i中，其他往后串 memmove
		char * d0, *d1;
		d1 = data + len * count;
		d0 = d1 - len;
		int i = count;
		while (i > n) {
			memmove(d1, d0, len);
			d0 -= len;
			d1 -= len;
			i--;
		}
		//插入
		d = data + len * n;
		memcpy(d, (char*) &v, valueLen);
		d += valueLen;
		memcpy(d, (char*) &k, keyLen);
	}
	count++;
	change = true;
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
//	memcpy((char*)&next, b, sizeof(next));
//	b += sizeof(next);

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
//	memcpy(b, (char*)&next, sizeof(next));
//	b += sizeof(next);

	memcpy(b, data, TREE_NODE_DATA_SIZE);

	//write to disk
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb+")) == NULL) {
		printf("FileNotFoundException");
		exit(0);
	}
	fseek(indexFile, self*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
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
	return k;
}

/*
 * 返回下标对应的关键词的指针（地址）
 */
template<typename key, typename value>
value TreeNode<key,value>::getValue(int index) {
	value v;
	const char * d = data;
	unsigned int len = keyLen + valueLen;
	d = d + len*index;
	memcpy((char*)&v, d, valueLen);
	return v;
}

template<typename key, typename value>
int TreeNode<key, value>::binarySearch(key k) {
	int low = 0;
	int high = count-1;
	while (low <= high) {
		int mid = (low+high)/2;
		if (k == getKey(mid)) {
			return mid;
		} else if (k < getKey(mid)) {
			high = mid - 1;
		} else {
			low = mid + 1;
		}
	}
	return -1;
}
/*
 * 返回值为value,即下一个子节点的指针
 */
template<typename key, typename value>
unsigned long int TreeNode<key, value>::getNextChild(key k) {
	for (int i = 0; i < count; i++) {	//count不可能为0，因为如果count小于某一个值后两个节点将会被合并
		key ki = getKey(i);
		if (ki >= k) {
			if (ki == k) {
				return getValue(i+1);
			} else {
				return getValue(i);
			}
		}
	}
	return getValue(count);
}

















