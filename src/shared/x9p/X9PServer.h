#pragma once
#include "X9P.h"
#include "X9PConnection.h"
#include "TCPSocket.h"

#include <vector>


class CBaseX9PServer
{
public:
	virtual void Begin(const char* ip, const char* port);
	virtual void End();

	virtual void AcceptConnections();
	virtual void ProcessPackets();

private:

	TCPListenSocket m_listener;
	std::vector<CX9PConnection> m_connections;

	int m_maxmessagesize;
	char* m_workingbuf;
};