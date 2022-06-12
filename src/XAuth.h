#pragma once
#include "xstring.h"

class X9PClient;

class XAuth
{
public:
	X9PClient* client;
	xstr_t uname;
	int id;
	
	inline bool HasGroup(xstr_t groupname) { return false; }


};


/*

checkperms(dirmode_t request)

	allowed = file_perms;
	if auth not owner
		allowed = allowed & ~owner_mask

	if auth not group
		allowed = allowed & ~group_mask

	return request & allowed

*/
