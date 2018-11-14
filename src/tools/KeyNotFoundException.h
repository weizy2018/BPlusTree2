/*
 * KeyNotFoundException.h
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#ifndef TOOLS_KEYNOTFOUNDEXCEPTION_H_
#define TOOLS_KEYNOTFOUNDEXCEPTION_H_

#include <exception>
#include <string>

using namespace std;

class KeyNotFoundException :public exception {
public:
	KeyNotFoundException();
	KeyNotFoundException(string message);
	virtual ~KeyNotFoundException();
public:
	const char * what () const throw ();
private:
	string message;
};

class FileNotFoundException :public exception {
public:
	FileNotFoundException();
	FileNotFoundException(string fileName);
	virtual ~FileNotFoundException();

	const char * what () const throw ();
private:
	string fileName;
};

#endif /* TOOLS_KEYNOTFOUNDEXCEPTION_H_ */
