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
 * 根结点
 */
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getRootNode() {
	try {
		rootNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(root);
	} catch (exception & e) {
		FILE * indexFile;
		if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
			throw FileNotFoundException("indexFileName");
		}
		char * block = (char*)malloc(BLOCK_SIZE);
		fseek(indexFile, root*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
		fread(block, BLOCK_SIZE, 1, indexFile);
		fclose(indexFile);

		rootNode = new TreeNode<key, value>(block, keyLen, valueLen, indexFileName);
		free(block);
		TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(root, rootNode);
		if (t) {
			delete t;
		}
	}
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
		handleDel(leafNode);
	}
}

template<typename key, typename value>
void BPlusTree<key, value>::handleDel(TreeNode<key, value> * leafNode) {
	rootNode = getRootNode();
	if (leafNode->getSelf() == rootNode->getSelf()) {	//只有根结点，就算再少也不作处理了
		return;
	}
	int minLeafData = (int)((treeNodeMaxSize - 1)*1.0 / 2 + 0.5);
	int minInnerData = (int)(treeNodeMaxSize*1.0 / 2 + 0.5);
	//父结点
	TreeNode<key, value> * parent = this->getParent(leafNode->getKey(0), leafNode->getSelf());
	if (parent->getSelf() == leafNode->getSelf()) {		//只有一个结点
		return;
	}
	//leafNode的左兄弟结点
	TreeNode<key, value> * leftNode = getLeftNode(leafNode, parent);
	//leafNode的右兄弟结点
	TreeNode<key, value> * rightNode = getRightNode(leafNode, parent);


	if (leftNode != nullptr && leftNode->getCount() > minLeafData) {	//先看能否向左借
		borrowLeft(leftNode, leafNode, parent);

	} else if (rightNode != nullptr && rightNode->getCount() > minLeafData) {	//是否能向右借
		borrowRight(right, leafNode, parent);
	} else if(leftNode != nullptr) {	//左右都不能借了，如果左边不为空，向左边合并
		mergeLeft(leftNode, leafNode, parent);
	} else {	//只能向右边合并
		//TreeNode<key, value> * rightNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent
		mergeRight(rightNode, leafNode, parent);
	}
}
/**
 * 向左边合并
 */
template<typename key, typename value>
void BPlusTree<key, value>::mergeLeft(TreeNode<key, value> * leftNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent) {
	//1. 把父结点value为leafNode->self()的项删除
	parent->delInnerData(leafNode->self());

	//2. 合并结点 将leafNode合并到leftNode中
	leftNode->addRightNodeData(leafNode);

	//3. 把文件中的最后一项用来填补被合并的结点，并将其父结点关于该项的value改为当前值，totalBlock -= 1
	if (leafNode->getSelf() != totalBlock - 1) {
		TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);
		TreeNode<key, value> * lastNodeParent = this->getParent(lastNode->getKey(0), lastNode->getSelf());
		char * leafNodeData = leafNode->getData();
		char * lastNodeData = lastNode->getData();
		int lastNodeCount = lastNode->getCount();

		memcpy(leafNodeData, lastNodeData, TREE_NODE_DATA_SIZE);
		leafNode->setChange(true);
		leafNode->setCount(lastNode->getCount());

		//如果最后一个结点是根结点所在的话
		rootNode = getRootNode();
		if (rootNode->getSelf() == lastNode->getSelf()) {
//			rootNode = leafNode;
			root = leafNode->getSelf();
		} else {
			lastNodeParent->updataValue(lastNode->getSelf(), leafNode->getSelf());
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(leafNode->getSelf(), leafNode);
			if (t) {
				delete t;
			}
			t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(lastNodeParent->getSelf(), lastNodeParent);
			if (t) {
				delete t;
			}
		}
		delete lastNode;
		totalBlock -= 1;
	} else {
		totalBlock -= 1;
	}
	//4. 递归对父结点进行该过程。。。
	int minInnerCount = (int)(treeNodeMaxSize*1.0/2 + 0.5);

	while(parent->getCount() < minInnerCount) {
		rootNode = getRootNode();
		if (parent->getSelf() == rootNode->getSelf()) {	//parent为根的情况
			if (parent->getCount() == 0) {
				if (parent->getSelf() != totalBlock - 1) {
					//把索引文件中的最后一个结点用来填rootNode所在的位置
					TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);
					parent->setCount(lastNode->getCount());
					parent->setChange(true);
					char * d0 = parent->getData();
					char * d1 = lastNode->getData();
					int count = lastNode->getCount();
					memcpy(d0, d1, TREE_NODE_DATA_SIZE);
					TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(parent->getSelf(), parent);
					if (t) {
						delete t;
					}

					//重置rootNode
					root = leftNode->getSelf();
					totalBlock -= 1;		//树变矮一层
					return;
				} else {	//rootNode为文件中的最后一个结点
					root = leftNode->getSelf();
					totalBlock -= 1;		//树变矮一层
					return;
				}
			} else {
				//do nothing
			}
		} else {	//parent不为根的情况
			bool merge = false;
			TreeNode<key, value> * node = parent;
			parent = this->getParent(parent->getKey(0), parent->getSelf());

			leftNode = getLeftNode(node, parent);
			TreeNode<key, value> * rightNode = getRightNode(node, parent);
			if (leftNode != nullptr && leftNode->getCount() > minInnerCount) {
				//从左边借一项
				this->borrowLeft(leftNode, node, parent);
			} else if (rightNode != nullptr && rightNode->getCount() > minInnerCount) {
				//从右边借一项
				this->borrowRight(rightNode, node, parent);
			} else if (leftNode != nullptr) {
				//向左合并
				//获取值为v的k
				key k = parent->delInnerData(node->getSelf());
				leftNode->addRightNodeData_Inner(node, k);
				merge = true;
			} else {
				//向右合并
				key k = parent->delInnerData(node->getSelf());
				rightNode->addLeftNodeData_Inner(node, k);
				merge = true;
			}

			if (merge) {		//如果发生合并，则将文件中的最后一块填到被合并的块中
				TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);
				node->setCount(lastNode->getCount());
				node->setChange(true);
				char * nodeData = node->getData();
				char * lastNodeData = lastNode->getData();
				memcpy(nodeData, lastNodeData, TREE_NODE_DATA_SIZE);
				//重新设置node的parent
				rootNode = getRootNode();
				if (rootNode->getSelf() == lastNode->getSelf()) {
//					rootNode = node;
					root = node->getSelf();
				} else {
					TreeNode<key, value> * lastNodeParent = this->getParent(lastNode->getKey(0), lastNode->getSelf());
					lastNodeParent->updataValue(lastNode->getSelf(), node->getSelf());
					TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(node->getSelf(), node);
					if (t) {
						delete t;
					}
				}
				delete lastNode;
				totalBlock--;
			}
		}
	}
}
/**
 * 向右边合并
 */
template<typename key, typename value>
void BPlusTree<key, value>::mergeRight(TreeNode<key, value> * rightNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent) {
	//0. 需要找到leafNode的左兄弟结点
	TreeNode<key, value> * leftLeftNode = getLeftNodeAll(leafNode, parent);

	//1. 把父结点value为leafNode->self()的项删除
	parent->delInnerData(leafNode->self());

	//2. 合并结点 将leafNode合并到leftNode中
	rightNode->addLeftNodeData(leafNode, leftLeftNode);

	//3. 把文件中的最后一项用来填补被合并的结点，并将其父结点关于该项的value改为当前值，totalBlock -= 1
	if (leafNode->getSelf() != totalBlock - 1) {
		TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);
		TreeNode<key, value> * lastNodeParent = this->getParent(lastNode->getKey(0), lastNode->getSelf());
		char * leafNodeData = leafNode->getData();
		char * lastNodeData = lastNode->getData();
		int lastNodeCount = lastNode->getCount();

		memcpy(leafNodeData, lastNodeData, TREE_NODE_DATA_SIZE);
		leafNode->setChange(true);
		leafNode->setCount(lastNode->getCount());

		//如果最后一个结点刚好是根结点的话
		rootNode = getRootNode();
		if (rootNode->getSelf() == lastNode->getSelf()) {
//			rootNode = leafNode;
			root = leafNode->getSelf();
		} else {
			lastNodeParent->updataValue(lastNode->getSelf(), leafNode->getSelf());
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(leafNode->getSelf(), leafNode);
			if (t) {
				delete t;
			}
			t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(lastNodeParent->getSelf(), lastNodeParent);
			if (t) {
				delete t;
			}
		}
		delete lastNode;
	} else {
		totalBlock -= 1;
	}
	//4. 递归进行。。。
	int minInnerCount = (int)(treeNodeMaxSize*1.0/2 + 0.5);
	while (parent->getCount() < minInnerCount) {
		rootNode = getRootNode();
		if (parent->getSelf() == rootNode->getSelf()) {		//parent刚好是根
			if (parent->getCount() == 0) {					//说明树根只剩一个子结点了,数个高度需要改变
				if (parent->getSelf() != totalBlock - 1) {	//根结点不是最后一个结点的情况
					//把索引文件中的最后一个结点用来填parent所在的位置,即rootNode的位置
					TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);	//此时lastNode就是上面的leafNode
					char * parentData = parent->getData();
					char * lastData = lastNode->getData();
					memcpy(parentData, lastData, TREE_NODE_DATA_SIZE);

					parent->setCount(lastNode->getCount());
					parent->setChange(true);
					TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(parent->getSelf(), parent);
					if (t) {
						delete t;
					}
					delete lastNode;
					//重置rootNode
					root = rightNode->getSelf();
					totalBlock -= 1;
					return;
				} else {	//根结点更好是最后一个结点
					root = rightNode->getSelf();
					totalBlock -= 1;
					return;
				}
			} else {	//根结点还有别的子结点
				//do nothing
			}
		} else {	//parent不是根的情况
			bool merge = false;
			TreeNode<key, value> * node = parent;
			parent = this->getParent(parent->getKey(0), parent->getSelf());

			TreeNode<key, value> * leftNode = getLeftNode(node, parent);
			rightNode = getRigthNode(node, parent);
			if (leftNode != nullptr && leftNode->getCount() > minInnerCount) {
				//从左边借一项
				this->borrowLeft(leftNode, node, parent);
			} else if (rightNode != nullptr && rightNode->getCount() > minInnerCount) {
				//从右边借一项
				this->borrowRight(rightNode, node, parent);
			} else if (leftNode != nullptr) {
				//向左合并
				key k = parent->delInnerData(node->getSelf());
				leftNode->addRightNodeData_Inner(node, k);
				merge = true;
			} else {
				key k = parent->delInnerData(node->getSelf());
				rightNode->addLeftNodeData_Inner(node, k);
				merge = true;
			}
			if (merge) {
				if (node->getSelf() == totalBlock - 1) {	//被合并的块刚好处在最后一块
					totalBlock -= 1;
				} else {
					TreeNode<key, value> * lastNode = getLastTreeNode(totalBlock);
					node->setCount(lastNode->getCount());
					node->setChange(true);
					char * nodeData = node->getData();
					char * lastNodeData = lastNode->getData();
					memcpy(nodeData, lastNodeData, TREE_NODE_DATA_SIZE);
					rootNode = getRootNode();
					if (rootNode->getSelf() == lastNode->getSelf()) {
						root = node->getSelf();
					} else {
						TreeNode<key, value> * lastNodeParent = this->getParent(lastNode->getKey(0), lastNode->getSelf());
						lastNodeParent->updataValue(lastNode->getSelf(), node->getSelf());
						TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(node->getSelf(), node);
						if (t) {
							delete t;
						}

					}
					delete lastNode;
					totalBlock--;
				}

			}
		}
	}

}

/**
 * 获取索引文件的最后一块
 */
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getLastTreeNode(unsigned long int totalBlock) {
	//rootNode为第0块
	TreeNode<key, value> * lastNode = nullptr;
	try {
		lastNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(totalBlock - 1);
	} catch (exception & e) {
		//需要从磁盘中读取
		FILE * indexFile;
		if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
			throw FileNotFoundException(string(indexFileName));
		}
		fseek(indexFile, (totalBlock - 1)*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
		char * block = (char*)malloc(BLOCK_SIZE);
		fread(block, BLOCK_SIZE, 1, indexFile);
		fclose(indexFile);
		//const char * block, int keyLen, int valueLen, const char * indexFileName
		lastNode = new TreeNode<key, value>(block, keyLen, valueLen, indexFileName);
	}

	return lastNode;
}

template<typename key, typename value>
void BPlusTree<key, value>::borrowLeft(TreeNode<key, value> * leftNode, TreeNode<key, value> * node, TreeNode<key, value> * parent){
	//node整体右窜一位,并将leftNode中的数据的最后一项加到node中
	node->moveRight(leftNode);
	//修改parent中的索引
	//1. 先获取移动后的leafNode的第一项的索引
	key k = node->getKey(0);
	//2. 将父结点value值为leafNode->getSelf()的项的索引置为k
	parent->updataKey(k, node->getSelf());
}
template<typename key, typename value>
void BPlusTree<key, value>::borrowRight(TreeNode<key, value> * rightNode, TreeNode<key, value> * node, TreeNode<key, value> * parent) {
	//将rightNode中的第一项加到leafNode的尾部
	node->moveLeft(rightNode);
	//修改parent中的索引
	//1. 获取右边结点的第一项的索引
	key k = rightNode->getKey(0);
	//2. 将父结点value值为rightNode->self(0的项的索引置为k
	parent->updataKey(k, rightNode->getSelf());
}
/**
 * 获取可以不具有同一个父结点的做兄弟结点
 */
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getLeftNodeAll(TreeNode<key, value> * node, TreeNode<key, value> * parent) {
	TreeNode<key, value> * leftNode;
	leftNode = getLeftNode(node, parent);
	if (leftNode == nullptr) {
		TreeNode<key, value> * parent_parent = getParent(parent->getKey(0), parent->getSelf());
		TreeNode<key, value> * parentLeftNode = getLeftNode(parent, parent_parent);
		value addr = parentLeftNode->getValue(parentLeftNode->getCount() + 1);
		try {
			leftNode = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->get(addr);
		} catch (exception & e) {
			FILE * indexFile;
			if ((indexFile = fopen(indexFileName, "rb")) == NULL) {
				throw FileNotFoundException(indexFileName);
			}
			char * block = (char*)malloc(BLOCK_SIZE);
			fseek(indexFile, addr*BLOCK_SIZE + OFFSET_LENGTH, SEEK_SET);
			fread(block, BLOCK_SIZE, 1, indexFile);
			leftNode = new TreeNode<key, value>(block, valueLen, indexFileName);
			free(block);
			TreeNode<key, value> * t = LRUCacheIndex<key, value>::getLruInst()->getLruCache()->put(addr, leftNode);
			if (t){
				delete t;
			}
			fclose(indexFile);
		}
	}
	return leftNode;
}
/**
 * 获取具有同一个父结点的左兄弟结点
 */
template<typename key, typename value>
TreeNode<key, value> * BPlusTree<key, value>::getLeftNode(TreeNode<key, value> * node, TreeNode<key, value> * parent) {
//	if (parent->getSelf() == rootNode->getSelf()) {		//只有一个叶结点，也是根结点
//		return nullptr;
//	}
	if (parent->getValue(0) == node->getSelf()) {						//node没有左兄弟结点
		return nullptr;
	}
	unsigned long int leftNodeAddr;
	for (int index = 0; index < parent->getCount(); index++) {
		value vv = parent->getValue(index);
		if (vv == node->getSelf()) {
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
TreeNode<key, value> * BPlusTree<key, value>::getRightNode(TreeNode<key, value> * node, TreeNode<key, value> * parent) {
	TreeNode<key, value> * rightNode = nullptr;
	unsigned long int rightNodeAddr;
//	if (node->getSelf() == rootNode->getSelf()) {		//只有一个叶结点，也是根结点
//		return nullptr;
//	}
	//获取rightNodeAddr
	for (int index = 0; index <= parent->getCount(); index++) {
		value vv = parent->getValue(index);
		if (vv == node->getSelf()) {
			if (index == parent->getCount()) {		//没有右兄弟结点
				return nullptr;
			}
			rightNodeAddr = parent->getValue(index + 1);
			break;
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
		fclose(indexFile);
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
	rootNode = getRootNode();
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
		rootNode = getRootNode();
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
	rootNode = getRootNode();
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
	rootNode = getRootNode();
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
	rootNode = getRootNode();
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
/**
 * 将键值为k的项删除（叶结点）
 */
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
/**
 * 将value为v的项删除（外结点）
 */
template<typename key, typename value>
key TreeNode<key, value>::delInnerData(value v) {
	char * d = data;
	int len = keyLen + valueLen;
	int index = 0;
	while (index <= count && getValue(index) != v) {
		index++;
	}
	if (index > count) {
		throw range_error("range out of index");
	}
	char * d0, *d1;
	//定位
	if (index == 0) {
		d0 = data;
		d1 = data + len;
	} else {
		d0 = data + len*index - keyLen;
		d1 = d0 + len;
	}
	//获取k
	key k;
	string str = typeid(string).name();
	char * kk;
	if (index == 0) {
		kk = d0 + valueLen;
	} else {
		kk = d0;
	}
	if (typeid(key).name() == str) {
		char * p = (char*)malloc(keyLen);
		memcpy(p, kk, keyLen);
		string s(p);
		k = s;
		free(p);
	} else {
		memcpy((char*)&k, kk, keyLen);
	}
	//移动
	for (int i = index; i < count; i++) {
		memmove(d0, d1, len);
		d0 += len;
		d1 += len;
	}
	count--;
	change = true;

	return k;
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
/**
 * 将右边的结点合并到自个身上,*****注意：如果是叶结点的话应该把相应的链接叶要复制
 * ******仅叶结点有效******
 */
template<typename key, typename value>
void TreeNode<key,value>::addRightNodeData(TreeNode<key, value> * rightNode) {
	int len = keyLen + valueLen;

	char * leftData = data + len*count;
	char * rightData = rightNode->getData();
	int rightCount = rightNode->getCount();

	int rightDataLen = rightCount*len;
	memcpy(leftData, rightData, rightDataLen);

	//设置数目
	count += rightCount;

	//设置叶结点的链接
	if (type == 1) {
		unsigned long int rightNodeNext = rightNode->getNext();
		this->setNext(rightNodeNext);
	}
	change = true;
}
/**
 * 将左边的结点合并到自个身上,右边的数据需要移动
 * ******仅叶结点有效******
 */
template<typename key, typename value>
void TreeNode<key,value>::addLeftNodeData(TreeNode<key, value> * leftNode, TreeNode<key, value> * leftLeftNode) {
	int len = keyLen + valueLen;
	int leftCount = leftNode->getCount();
	//自身数据右移leftCount个单位
	char * d0 = data + len*(count - 1);
	char * d1 = d0 + len*leftCount;
	//自身右移
	for (int i = 0; i < count; i++) {
		memmove(d1, d0, len);
		d0 -= len;
		d1 -= len;
	}

	//添加leftNode的数据
	char * rightData = data;
	char * leftData = leftNode->getData();
	memcpy(rightData, leftData, leftCount*len);

	//设置左左边的next
	leftLeftNode->setNext(self);
	leftLeftNode->setChange(true);

	count += leftCount;
	change = true;
}
/**
 *
 */
template<typename key, typename value>
void TreeNode<key,value>::addRightNodeData_Inner(TreeNode<key, value> * rightNode, key k) {
	int len = keyLen + valueLen;
	char * left = data + len*count + valueLen;

	//先把k加到自身后面
	string str = typeid(string).name();
	if (typeid(key).name() == str) {
		char * p = (char*)malloc(keyLen);
		char * s = k.c_str();
		memcpy(p, s, k.size());
		p[k.size()] = '\0';
		memcpy(left, p, keyLen);
		free(p);
		free(s);
	} else {
		memcpy(left, (char*)&k, keyLen);
	}
	left += keyLen;
	int rightCount = rightNode->getCount();
	char * rightData = rightNode->getData();
	memcpy(left, rightData, rightCount*len + valueLen);
	count += (rightCount + 1);
	change = true;
}
/**
 *
 */
template<typename key, typename value>
void TreeNode<key,value>::addLeftNodeData_Inner(TreeNode<key, value> * leftNode, key k) {
	int len = keyLen + valueLen;
	int leftCount = leftNode->getCount();
	//数据先右移 右移leftCount+1个
	char * d0 = data + len*(count - 1);
	char * d1 = d0 + len*(leftCount + 1);
	for (int i = 0; i < count; i++) {
		memmove(d1, d0, len);
		d0 -= len;
		d1 -= len;
	}
	//把leftNode数据从头放
	d0 = data;
	char * leftData = leftNode->getData();
	memcpy(d0, leftData, len*leftCount + valueLen);		//注意这个valueLen
	d0 += len*leftCount;
	//之后再放k
	string str = typeid(string).name();
	if (typeid(key).name() == str) {
		char * p = (char*)malloc(keyLen);
		char * s = k.c_str();
		memcpy(p, s, k.size());
		p[k.size()] = '\0';
		memcpy(d0, p, keyLen);
		free(p);
		free(s);
	} else {
		memcpy(d0, (char*)&k, keyLen);
	}
	count += (leftCount + 1);
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
/**
 * 从左边结点（leftNode)移动一位数据到右边
 */
template<typename key, typename value>
void TreeNode<key, value>::moveRight(TreeNode<key, value> * leftNode) {
	int len = keyLen + valueLen;
	char * d1 = data + len*(count + 1);		//如果是内节点的话需要移动count+1项，不是的话都行
	char * d0 = d1 - len;
	//d0 >>> d1
	for (int i = 0; i <= count; i++) {
		memmove(d1, d0, len);
		d0 -= len;
		d1 -= len;
	}
	char * leftData = leftNode->getData();
	int leftCount = leftNode->getCount();
	//左边结点最右边的一项数据
	char * left = leftData + len*(leftCount - 1);
	d0 = data;
	memcpy(d0, left, len);

	//改变数据的个数
	leftNode->setCount(leftCount - 1);
	count++;
	//改动标志
	change = true;
	leftNode->setChange(true);
}
/**
 * 从右边结点（rightNode）移动一位数据到左边
 */
template<typename key, typename value>
void TreeNode<key, value>::moveLeft(TreeNode<key, value> * rightNode) {
	int len = keyLen + valueLen;
	char * d;
	if (type == 1){
		d = data + len*count;
	} else {
		d = data + len*count + valueLen;
	}
	char * rightData = rightNode->getData();
	//移动数据
	memcpy(d, rightData, len);

	//右边结点整体左移一位
	char * d0 = rightData;
	char * d1 = rightData + len;
	int rightCount = rightNode->getCount();
	for (int i = 0; i < rightCount; i++) { //如果是非叶结点的话需要移动rightCount次，不是的话也行
		memmove(d0, d1, len);
		d0 += len;
		d1 += len;
	}
	//改变两结点数据个数
	count += 1;
	rightNode->setCount(rightCount - 1);
	//改动标志
	change = true;
	rightNode->setChange(true);
}
/**
 * 将值为v的项的索引置为k
 */
template<typename key, typename value>
void TreeNode<key, value>::updataKey(key k, value v) {
	int n = 0;
	while(n <= count && this->getValue(n) != v) {
		n++;
	}
	int len = keyLen + valueLen;
	char * d = data + len*(n - 1) + valueLen;
	//替换k
	string str = typeid(string).name();
	if (typeid(key).name() == str) {
		char * kk = (char*)malloc(keyLen);
		char * a = k.c_str();
		memcpy(kk, a, k.size());
		kk[k.size()] = '\0';
		memcpy(d, kk, keyLen);
		free(kk);
		free(a);
	} else {
		memcpy(d, (char*)&k, keyLen);
	}
}
/**
 * 将 v0 改成 v1
 */
template<typename key, typename value>
void TreeNode<key, value>::updataValue(value v0, value v1) {
	int index = 0;
	while (index <= count && getValue(index) != v0) {
		index++;
	}
	int len = keyLen + valueLen;
	char * d = data + len*index;
	memcpy(d, (char*)&v1, valueLen);
	change = true;
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



















