#pragma once
#include "xstring.h"

class X9PClient;

class XAuth
{
public:
	X9PClient* client;
	xstr_t uname;
	int id;
};
