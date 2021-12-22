#pragma once

#include "ISocket.h"
#include <cstdint>


class TCPClientSocket : public IClientSocket
{
public:
	TCPClientSocket();
	void Close();

	
	ISOCKET_RESULT Connect(const char* hostname, const char* servicename);

	ISOCKET_RESULT Recv(char* buf, int buflen, int& bytesWritten);
	ISOCKET_RESULT Send(char* buf, size_t buflen);

private:
	
	// This is for when we get a client off the listener
	friend class TCPListenSocket;
	TCPClientSocket(uintptr_t socket);

	uintptr_t m_socket;
};



class TCPListenSocket : public IListenSocket
{
public:
	TCPListenSocket();
	void Close();

	ISOCKET_RESULT Listen(const char* hostname, const char* servicename);


	ISOCKET_RESULT Accept(TCPClientSocket& out);
private:

	uintptr_t m_socket;
};
