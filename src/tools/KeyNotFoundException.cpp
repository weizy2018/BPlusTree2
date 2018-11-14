/*
 * KeyNotFoundException.cpp
 *
 *  Created on: Nov 14, 2018
 *      Author: weizy
 */

#include "KeyNotFoundException.h"

KeyNotFoundException::KeyNotFoundException() {
	message = "KeyNotFoundException";

}
KeyNotFoundException::KeyNotFoundException(string message) {
	this->message = message;
}

KeyNotFoundException::~KeyNotFoundException() {
	// TODO Auto-generated destructor stub
}

const char * KeyNotFoundException::what() const {
	message = "KeyNotFoundException:" + message + " not found";
	return message.c_str();
}

//------------------------------------------
//------------------------------------------
FileNotFoundException::FileNotFoundException() {
	fileName = "";

}
FileNotFoundException::FileNotFoundException(string fileName) {
	this->fileName = fileName;
}

FileNotFoundException::~FileNotFoundException() {
	// TODO Auto-generated destructor stub
}

const char * FileNotFoundException::what() const throw() {
	string error = "File" + fileName + " not found";
	return error.c_str();
}

