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
using namespace std;

#define BLOCK_SIZE 				4*1024
#define OFFSER_LENGTH 			16
#define TREE_NODE_DATA_SIZE 	4*1024-32

template<typename key, typename value>
class BPlusTree {
public:
	BPlusTree(const char * indexFileName, int keyLen, int valueLen, bool create);
	virtual ~BPlusTree();
public:
	void init();					//用于初始化文件中已经存在了的b+树
	void createIndex();				//发布create index后用于初始化b+树
	void put(key k, value v);
	value get(key k);
	void remove(key k);
private:
	const char * indexFileName;
	int keyLen;
	int valueLen;
	char * head;					//索引文件头部保存的信息 以下定义的内容

	unsigned long int totalBlock;	//叶节点和内节点的总数
	unsigned long int root;			//根节点所在位置

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
	unsigned long int parent;	//该节点的父节点	-1表示根节点
	unsigned long int next;		//if(type==1) next为下一叶节点 0为内节点的下一节点 -1表示没有下一节点
	int type;					//叶节点： type==1	非叶节点：type==0
	unsigned int count;			//该块数据域中有多少个数据

	int keyLen;					//key的长度
	int valueLen;				//value的长度
	const char * indexFileName;
public:
	TreeNode(int keyLen, int valueLen, unsigned long int self, unsigned long int parent, int type, const char * indexFileName);
	TreeNode(const char * block, int keyLen, int valueLen, const char * indexFileName);
	~TreeNode();
public:
	unsigned long int getSelf();
	unsigned long int getParent();
	unsigned long int getNext();
	int getType();
	int getCount();
	key getKey(int index);
	value getValue(int index);
public:
	void addData(key k, value v);

public:
	void parsed(const char * block);	//将从磁盘读取的块进行转换
	void writeBack();			//写回磁盘

};



#endif /* BPLUSTREE_H_ */
