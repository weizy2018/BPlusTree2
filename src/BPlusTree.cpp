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
/**
 * 插入结点
 */
template<typename key, typename value>
void BPlusTree<key, value>::put(key k, value v) {
	//找到应该包含k值的叶结点
	TreeNode<key, value> * leafNode = getLeafNode(k);
	//往叶结点中插入数据k，v
	leafNode->addData(k, v);
	if (leafNode->getCount() >= treeNodeMaxSize) {
		//分裂
		split(leafNode);
	}

}
/**
 * 删除结点
 */
template<typename key, typename value>
void BPlusTree<key, value>::remove(key k, value v) {
	TreeNode<key, value> * leafNode = getLeafNode(k);
	try {
		leafNode->delData(k, v);
	} catch (exception & e) {
		//KeyNotFoundException
		return;
	}
	int minLeafData = (int)((treeNodeMaxSize - 1)*1.0 / 2 + 0.5);	//向上取整 等价于ceil((treeNodeMaxSize-1))/2
	if (leafNode->getCount() < minLeafData) {
		handleDel(leafNode, k, v);
	}
}

template<typename key, typename value>
void BPlusTree<key, value>::handleDel(TreeNode<key, value> * leafNode, key k, value v) {
	if (leafNode->getSelf() == rootNode->getSelf()) {	//只有根结点，就算再少也不作处理了
		return;
	}
	//leafNode的左兄弟结点

	//leafNode的右兄弟结点

	TreeNode<key, value> * parent = this->getParent(k ,v);


}
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getLeftNode(TreeNode<key, value> * node, key k, value v) {
	TreeNode<key, value> * parent = this->getParent(k, v);
	if (parent->getValue(0) == v) {		//node没有左兄弟结点
		return nullptr;
	}
	unsigned long int leftNodeAddr;
	for (int index = 1; index < parent->getCount(); index++) {
		value vv = parent->getValue(index);
		if (vv == v) {
			leftNodeAddr = parent->getValue(index-1);
			break;
		}
	}

	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		throw FileNotFoundException(indexFileName);
	}
	TreeNode<key, value> * leftNode = nullptr;
	try {
		leftNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(leftNodeAddr);
	} catch (exception & e) {
		char * block = (char*)malloc(BLOCK_SIZE);
		fseek(indexFile, leftNodeAddr*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
		fread(block, BLOCK_SIZE, 1, indexFile);
		leftNode = new TreeNode<key, value>(block, valueLen, indexFileName);
		free(block);

		TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(leftNodeAddr, leftNode);
		if (t) {
			delete t;
		}
	}
	fclose(indexFile);
	return leftNode;
}
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getRightNode(TreeNode<key, value> * node, key k, value v) {
	TreeNode<key, value> * rightNode = nullptr;
	unsigned long int rightNodeAddr;
	//获取rightNodeAddr
	if (node->getType() == 1) {	//叶结点
		rightNodeAddr = node->getNext();
	} else {	//非叶结点
		TreeNode<key, value> * parent = this->getParent(k, v);
		for (int index = 0; index <= parent->getCount(); index++) {
			value vv = parent->getValue(index);
			if (vv == v) {
				if (index == parent->getCount()) {		//没有右兄弟结点
					return nullptr;
				}
				rightNodeAddr = parent->getValue(index + 1);
				break;
			}
		}
	}
	//从rightNodeAddr中获取结点TreeNode
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		throw FileNotFoundException(indexFileName);
	}
	try {
		rightNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(rightNodeAddr);
	} catch (exception & e) {
		char * block = (char*) malloc(BLOCK_SIZE);
		fseek(indexFile, rightNodeAddr * BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
		fread(block, BLOCK_SIZE, 1, indexFile);
		rightNode = new TreeNode<key, value>(block, valueLen, indexFileName);
		free(block);

		TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(rightNodeAddr, rightNode);
		if (t) {
			delete t;
		}
	}
	return rightNode;
}
template<typename key, typename value>
void BPlusTree<key, value>::split(TreeNode<key, value> * leafNode) {

	unsigned long int self = totalBlock;
	totalBlock++;
	TreeNode<key, value> * rightNode = new TreeNode<key, value>(keyLen, valueLen, self, 1, indexFileName);
	pair<key, value> p = leafNode->splitData(rightNode);	//返回分裂后父结点的key以及叶结点的地址
	//将分裂后的结点放入LRU中
	TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(leafNode->getSelf(), leafNode);
	if (t) {
		delete t;
	}
	t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(rightNode->getSelf(), rightNode);
	if (t) {
		delete t;
	}

	//找分裂后左边结点的父结点,通过该结点的第一个关键词以及自身的块号（id）
	TreeNode<key, value> * parentNode = getParent(leafNode->getKey(0), leafNode->getSelf());
	if (parentNode->getSelf() == leafNode->getSelf()) {		//就一个根结点，即根结点也为叶结点
		rootNode->setCount(0);
		rootNode->setSelf(totalBlock);
		rootNode->setType(0);
		//rootNode->addData(p.first, p.second);
		rootNode->addFirstInnerData(leafNode->getSelf(), p.first, p.second);

		//更新索引文件头部信息
		totalBlock++;									//总块数
		root = rootNode->getSelf();						//根结点的位置
		rootNode->writeBack();							//把跟结点写回磁盘
		return;
	}
	//根结点为非内节点
	parentNode->addInnerData(p.first, p.second);

	while (parentNode->getCount() >= treeNodeMaxSize) {
		if (parentNode->getSelf() == rootNode->getSelf()) {	//分裂的结点为根结点
			rootNode->setCount(0);
			rootNode->setSelf(totalBlock);
			rootNode->setType(0);
			//rootNode->addData(p.first, p.second);
			rootNode->addFirstInnerData(leafNode->getSelf(), p.first, p.second);

			//更新索引文件头部信息
			totalBlock++;									//总块数
			root = rootNode->getSelf();						//根结点的位置
			break;
		} else {
			self = totalBlock;
			totalBlock++;
			TreeNode<key, value> * rightInnerNode = new TreeNode<key, value>(keyLen, valueLen, self, 0, indexFileName);
			pair<key, value> inner = parentNode->splitInnerData(rightInnerNode);
			//---------放入LRU缓冲区中---------
			t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(parentNode->getSelf(), parentNode);
			if (t) {
				delete t;
			}
			t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(rightInnerNode->getSelf(), rightInnerNode);
			if (t) {
				delete t;
			}

			parentNode = getParent(parentNode->getKey(0), parentNode->getSelf());
			parentNode->addInnerData(p.first, p.second);
		}
	}
}

template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getParent(key k, value childId) {
	if (rootNode->getSelf() == childId) {	//只有根结点
		return rootNode;
	}
	FILE * indexFile;
	if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
		throw FileNotFoundException(indexFileName);
	}
	TreeNode<key, value> * node = rootNode;
	TreeNode<key, value> * childNode = nullptr;
	unsigned long int nextAddr = node->getNextChild(k);
	while (nextAddr != childId) {
		try {
			childNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(nextAddr);
		} catch (exception & e) {
			char * child = (char*) malloc(BLOCK_SIZE);

			fseek(indexFile, nextAddr*BLOCK_SIZE+OFFSET_LENGTH, SEEK_SET);
			fread(child, BLOCK_SIZE, 1, indexFile);
			childNode = new TreeNode<key, value>(child, keyLen, valueLen,indexFileName);
			free(child);
			//放入LRU缓冲区
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(nextAddr, childNode);
			if (t) {
				delete t;
			}
		}
		node = childNode;
		nextAddr = node->getNextChild(k);
	}
	return node;
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
			childNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(nextAddr);

		} catch (exception & e) {	//缓冲区没找到
			char * child = (char*) malloc(BLOCK_SIZE);
			fseek(indexFile, nextAddr*BLOCK_SIZE+OFFSET_LENGTH, SEEK_SET);
			fread(child, BLOCK_SIZE, 1, indexFile);
			childNode = new TreeNode<key, value>(child, keyLen, valueLen,indexFileName);
			free(child);
			//放入LRU缓冲区
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(nextAddr, childNode);
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
			childNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(nextAddr);
		} catch (exception & e) {
			char * child = (char*)malloc(BLOCK_SIZE);
			fread(child, BLOCK_SIZE, 1, indexFile);
			childNode = new TreeNode<key, value>(child, keyLen, valueLen, indexFileName);
			free(child);
			//放入LRU缓冲区
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(nextAddr, childNode);
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
TreeNode<key,value>::TreeNode(int keyLen, int valueLen, unsigned long int self,int type, const char * indexFileName) {
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	this->self = self;
//	this->parent = parent;
	this->type = type;
	this->count = 0;
//	this->next = -1;
	this->indexFileName = indexFileName;
	this->setNext(-1);
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
	change = false;
	treeNodeMaxSize = (TREE_NODE_DATA_SIZE - valueLen)/(keyLen + valueLen);
}
template<typename key, typename value>
TreeNode<key,value>::TreeNode(const char * block, int keyLen, int valueLen, const char * indexFileName) {
	this->keyLen = keyLen;
	this->valueLen = valueLen;
	this->indexFileName = indexFileName;
	data = (char*)malloc(TREE_NODE_DATA_SIZE);
	change = false;
	treeNodeMaxSize = (TREE_NODE_DATA_SIZE - valueLen)/(keyLen + valueLen);
	this->setNext(-1);
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
		memcpy(d, (char*) &v, valueLen);		//value都是unsigned long int类型
		d += valueLen;
		string str = typeid(string).name();
		if (typeid(key).name() == str) {		//string 类型
			char * kk = (char*)malloc(keyLen);
			char *a = k.c_str();
			memcpy(kk, a, k.size());
			kk[k.size()] = '\0';
			memcpy(d, kk, keyLen);
			free(kk);
			free(a);
		} else {
			memcpy(d, (char*) &k, keyLen);		//其他类型 int、float、double
		}

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

		string str = typeid(string).name();
		if (typeid(key).name() == str) {		//string 类型
			char * kk = (char*)malloc(keyLen);
			char *a = k.c_str();
			memcpy(kk, a, k.size());
			kk[k.size()] = '\0';
			memcpy(d, kk, keyLen);
			free(kk);
			free(a);
		} else {
			memcpy(d, (char*) &k, keyLen);
		}
	}
	count++;
	change = true;
}
template<typename key, typename value>
void TreeNode<key, value>::delData(key k, value v) {
	char * d = data;
	int index = 0;
	while(index<count && getKey(index) != k) {
		index++;
	}
	//没找到
	if (index >= count) {
		throw KeyNotFoundException(k);
	}
	int len = keyLen + valueLen;
	if (index == (count - 1)) {
		count--;
	} else {
		char * d0 = data + len*index;
		char * d1 = d0 + len;
		for (int i = index; i < count; i++) {
			memmove(d0, d1, len);
			d0 += len;
			d1 += len;
		}
	}
	change = true;
}
template<typename key, typename value>
void TreeNode<key, value>::addInnerData(key k, value v) {
	unsigned int len = keyLen + valueLen;
	int n = 0;
	while (n < count && getKey(n) < k) {
		n++;
	}
	//插入位置i中，其他往后串 memmove
	char * d0, *d1;
	d1 = data + len * count + valueLen;
	d0 = d1 - len;
	int i = count;
	while (i > n) {
		memmove(d1, d0, len);
		d0 -= len;
		d1 -= len;
		i--;
	}

	char * d = data + len * n + valueLen;

	//先放key
	string str = typeid(string).name();
	if (typeid(key).name() == str) {		//string 类型
		char * kk = (char*)malloc(keyLen);
		char *a = k.c_str();
		memcpy(kk, a, k.size());
		kk[k.size()] = '\0';
		memcpy(d, kk, keyLen);
		free(kk);
		free(a);
	} else {
		memcpy(d, (char*) &k, keyLen);
	}
	d += keyLen;
	//后放value
	memcpy(d, (char*)&v, valueLen);

	count++;
	change = true;
}
template<typename key, typename value>
void TreeNode<key, value>::addFirstInnerData(value left, key k, value right) {
	char * d = data;
	memcpy(d, (char*) &left, valueLen);
	d += valueLen;
	string str = typeid(string).name();
	if (typeid(key).name() == str) {			//string 类型
		char * kk = (char*) malloc(keyLen);
		char *a = k.c_str();
		memcpy(kk, a, k.size());
		kk[k.size()] = '\0';
		memcpy(d, kk, keyLen);
		free(kk);
		free(a);
	} else {
		memcpy(d, (char*) &k, keyLen);		//其他类型 int、float、double
	}
	d += keyLen;
	memcpy(d, (char*)&right, valueLen);
	count = 1;
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
//	memcpy((char*)&parent, b, sizeof(parent));
//	b += sizeof(parent);

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
//	memcpy(b, (char*)&parent, sizeof(parent));
//	b += sizeof(parent);

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
char * TreeNode<key, value>::getData() {
	return data;
}
template<typename key, typename value>
unsigned long int TreeNode<key,value>::getSelf() {
	return self;
}
//template<typename key, typename value>
//unsigned long int TreeNode<key,value>::getParent() {
//	return parent;
//}

template<typename key, typename value>
int TreeNode<key,value>::getType() {
	return type;
}

template<typename key, typename value>
int TreeNode<key,value>::getCount() {
	return count;
}
template<typename key, typename value>
void TreeNode<key, value>::setCount(int count) {
	this->count = count;
}
template<typename key, typename value>
void TreeNode<key, value>::setChange(bool change) {
	this->change = change;
}
template<typename key, typename value>
bool TreeNode<key, value>::getChange() {
	return change;
}
template<typename key, typename value>
key TreeNode<key,value>::getKey(int index) {
	key k;
	const char * d = data;
	unsigned int len = keyLen + valueLen;
	d = d + len*index;
	d += valueLen;
	string str = typeid(string).name();
	if (typeid(key).name() == str) {
		char * p = (char*)malloc(keyLen);
		memcpy(p, d, keyLen);
		string str(p);
		delete p;
		k = str;
	} else {
		memcpy((char*)&k, d, keyLen);
	}
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
/*
 * 叶结点分裂
 */
template<typename key, typename value>
pair<key, value> TreeNode<key, value>::splitData(TreeNode<key, value> * right) {
	char * rightData = right->getData();
	unsigned long int next = this->getNext();

	int leftCount = count/2;
	int len = keyLen + valueLen;
	char * d = data + leftCount*len;
	memcpy(rightData, d, (count-leftCount)*len);	//将原有数据复制一半到新的结点中

	right->setCount(count-leftCount);				//跟新右边数据的个数  先
	count = leftCount;								//更新左边数据的个数  后

	right->setNext(next);							//右边部分的next应该是原有数据的next
	this->setNext(right->getSelf());				//左边部分的next应该指向新的结点

	pair<key, value> p;								//分裂后产生一组数据插入父结点中
	p.first = right->getKey(0);						//键值为该结点第一个键的键值
	p.second = right->getSelf();					//数据域应该是该结点的id

	this->setChange(true);
	right->setChange(true);

	return p;
}
/**
 *非叶结点分裂
 */
template<typename key, typename value>
pair<key, value> TreeNode<key, value>::splitInnetData(TreeNode<key, value> * right) {
	char * rightData = right->getData();

	pair<key, value> p;
	p.fist = this->getKey(count/2);
	p.second = right->getSelf();

	int leftCount = count/2;
	int len = keyLen + valueLen;

	char * leftData = data + valueLen + len*leftCount + keyLen;
	memcpy(rightData, leftData, valueLen);
	leftData += valueLen;
	memcpy(rightData, leftData, len*(count-leftCount-1));

	right->setCount(count-leftCount);
	this->count = leftCount;

	return p;
}
//叶结点的下一个结点
template<typename key, typename value>
unsigned long int TreeNode<key, value>::getNext() {
	int len = keyLen + valueLen;
	const char * d = data + treeNodeMaxSize*len;
	unsigned long int next;
	memcpy((char*)&next, d, sizeof(next));
	return next;
}
template<typename key, typename value>
void TreeNode<key, value>::setNext(unsigned long int next) {
	int len = keyLen + valueLen;
	char * d = data + treeNodeMaxSize*len;
	memcpy(d, (char*)&next, sizeof(next));
}



















