/*
 * BPlusTree.h
 *
 *  Created on: Nov 13, 2018
 *      Author: weizy
 */

#ifndef BPLUSTREE_H_
#define BPLUSTREE_H_

#include <string>
#include <string.h>
#include <stdlib.h>
#include <exception>
#include <typeinfo>
#include "tools/KeyNotFoundException.h"
#include "tools/LRUCacheIndex.h"

using namespace std;

#define BLOCK_SIZE 				4*1024
#define TREE_NODE_HEAD_SIZE		16
#define TREE_NODE_DATA_SIZE 	(4*1024-16)
#define OFFSET_LENGTH			(2*sizeof(unsigned long int))

template<typename key, typename value>
class TreeNode;

template<typename key, typename value>
class BPlusTree {
public:
	BPlusTree(const char * indexFileName, int keyLen, int valueLen, bool create);
	virtual ~BPlusTree();
public:
	void init();					//用于初始化文件中已经存在了的b+树
	void createIndex();				//发布create index后用于初始化b+树
	void put(key k, value v);
	value get(key k);				//应该是undigned long int的，但是因为找不到的时候返回-1，索引只能是有符号的整数了
									//后期可改为抛出异常进行处理
	void remove(key k, value v);
private:
	TreeNode<key, value> * getLeafNode(key k);
	TreeNode<key, value> * getParent(key k, value childId);
	void split(TreeNode<key, value> *);
	void handleDel(TreeNode<key, value> * leafNode);
	TreeNode<key, value> * getLeftNode(TreeNode<key, value> * node, TreeNode<key, value> * parent);
	TreeNode<key, value> * getLeftNodeAll(TreeNode<key, value> * node, TreeNode<key, value> * parent);
	TreeNode<key, value> * getRightNode(TreeNode<key, value> * node, TreeNode<key, value> *);
	void borrowLeft(TreeNode<key, value> * leftNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent);
	void borrowRight(TreeNode<key, value> * rightNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent);
	void mergeLeft(TreeNode<key, value> * leftNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent);
	void mergeRight(TreeNode<key, value> * rightNode, TreeNode<key, value> * leafNode, TreeNode<key, value> * parent);
	TreeNode<key, value> * getLastTreeNode(unsigned long int totalBlock);
private:
	const char * indexFileName;
	int keyLen;
	int valueLen;
	int treeNodeMaxSize;			//结点所能存放数据的做大值
	char * head;					//索引文件头部保存的信息 以下定义的内容
	unsigned long int totalBlock;	//叶节点和内节点的总数
	unsigned long int root;			//根节点所在位置

	//rootNode
	TreeNode<key, value> * rootNode;
private:
	TreeNode<key, value> * getRootNode();

};

template<typename key, typename value>
class TreeNode {
private:
	//块中的数据域
	char * data;				//包含key和Value  大小BLOCK_SIZE	value为key所在的块号,key的长度应该是固定的
	//-------------------data数据的存放方式-----------------------------------
	//point key| point key|...|point key| point		最后面这个point指向下一个叶节点
	//----------------------------------------------------------------------
	//块头的定义
	unsigned long int self;		//本身在第几块
//	unsigned long int parent;	//该节点的父节点	-1表示根节点
//	unsigned long int next;		//if(type==1) next为下一叶节点 0为内节点的下一节点 -1表示没有下一节点
	int type;					//叶节点： type==1	非叶节点：type==0
	unsigned int count;			//该块数据域中有多少个数据
	int treeNodeMaxSize;		//块最大数据个数

	int keyLen;					//key的长度
	int valueLen;				//value的长度
	const char * indexFileName;

	bool change;				//更改标志
public:
	TreeNode(int keyLen, int valueLen, unsigned long int self, int type, const char * indexFileName);
	TreeNode(const char * block, int keyLen, int valueLen, const char * indexFileName);
	~TreeNode();
public:
	char * getData();
	unsigned long int getSelf();
//	unsigned long int getParent();
	unsigned long int getNext();
	void setNext(unsigned long int next);
	int getType();
	int getCount();
	void setCount(int count);
	void setChange(bool change);
	bool getChange();
	key getKey(int index);
	value getValue(int index);
public:
	void addData(key k, value v);
	void delData(key k, value v);			//将键值为k的项删除（叶结点）
	key delInnerData(value v);				//将值为v的项删除
	void addInnerData(key, value);

	void addRightNodeData(TreeNode<key, value> * rightNode);	//将右边的结点合并到自个身上
	void addLeftNodeData(TreeNode<key, value> * leftNode, TreeNode<key, value> * leftLeftNode);	//将左边的结点合并到自个身上

	void addRightNodeData_Inner(TreeNode<key, value> * rightNode, key k);
	void addLeftNodeData_Inner(TreeNode<key, value> * leftNode, key k);

	void addFirstInnerData(value left, key k, value right);

	int binarySearch(key k);
	unsigned long int getNextChild(key k);
	pair<key, value> splitData(TreeNode<key, value> * right);		//叶结点分裂  返回右边数据的个数
	pair<key, value> splitInnetData(TreeNode<key, value> * right);	//非叶结点分裂
	void moveRight(TreeNode<key, value> * leftNode);				//从左边结点移动一位数据到右边
	void moveLeft(TreeNode<key, value> * rightNode);				//从右边结点移动一位数据到左边
	void updataKey(key k, value v);									//将值为v的项的索引置为k
	void updataValue(value v0, value v1);							//将v0改成v1
private:

public:
	void parsed(const char * block);	//将从磁盘读取的块进行转换
	void writeBack();			//写回磁盘

};



#endif /* BPLUSTREE_H_ */
