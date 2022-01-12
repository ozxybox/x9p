#pragma once
#include "TCPSocket.h"
#include <vector>

#include "X9PFid.h"


class CX9PUser
{
public:

//private:
	char* m_uname;
	char* m_aname;
};

class CX9PConnection
{
public:
	CX9PConnection(TCPClientSocket client);

	//private:

	TCPClientSocket m_socket;

	std::vector<CX9PUser> m_users;
//	std::vector<fid_t, > m_fids;

	int m_agreedsize;
};