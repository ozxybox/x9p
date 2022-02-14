#pragma once
#include "X9P.h"
#include "X9PFileSystem.h"
#include "X9PClient.h"
#include "socket/TCPSocket.h"

#include <vector>



class X9PServer
{
public:
	void Begin(const char* ip, const char* port, X9PFileSystem* fs);
	void End();

	void AcceptConnections();
	void ProcessPackets();

private:

	TCPListenSocket m_listener;
	std::vector<X9PClient> m_clients;

	uint32_t m_maxmessagesize;
	char* m_respbuffer;

	X9PFileSystem* m_filesystem;
};