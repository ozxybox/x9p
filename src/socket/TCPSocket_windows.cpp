#include "TCPSocket.h"

#include "XLogging.h"

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

static bool s_WinSockHasNotInitalized = true;
// 0: OK
int InitWinsock()
{
	int err = 0;

	if (s_WinSockHasNotInitalized)
	{
		WSADATA wsadata;
		err = WSAStartup(MAKEWORD(2, 2), &wsadata);
			
        s_WinSockHasNotInitalized = false;
	}

	return err;
}
void ShutdownWinsock()
{
    s_WinSockHasNotInitalized = true;
    WSACleanup();
}

///////////////////////////////////////////////
// TODO: Move this elsewhere at a later date //
///////////////////////////////////////////////
ISOCKET_RESULT SystemInitSocket()
{
    int err = InitWinsock();
    if (err)
        return ISOCKET_RESULT::STARTUP_FAIL;
    return ISOCKET_RESULT::OK;
}
///////////////////////////////////////////////
// TODO: Move this elsewhere at a later date //
///////////////////////////////////////////////
ISOCKET_RESULT SystemShutdownSocket()
{
    ShutdownWinsock();
    return ISOCKET_RESULT::OK;
}


ISOCKET_RESULT CreateSocket(SOCKET& out, const char* hostname, const char* servicename, bool listener)
{
    int err;
    struct addrinfo* addr = NULL;
    struct addrinfo hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the address and port
    err = getaddrinfo(hostname, servicename, &hints, &addr);
    if (err)
        return ISOCKET_RESULT::RESOLVE_ADDRESS_FAIL;

    // Create the socket
    SOCKET sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
    if (sock == INVALID_SOCKET)
    {
        freeaddrinfo(addr);
        return ISOCKET_RESULT::CREATE_FAIL;
    }

    // Listen socker or client socket?
    if (listener)
    {
        // Bind the socket to the address
        err = bind(sock, addr->ai_addr, (int)addr->ai_addrlen);
        if (err == SOCKET_ERROR)
        {
            freeaddrinfo(addr);
            closesocket(sock);
            return ISOCKET_RESULT::BIND_FAIL;
        }

        // Setup the listen socket
        int err = listen(sock, SOMAXCONN);
        if (err)
        {
            closesocket(sock);
            return ISOCKET_RESULT::LISTEN_FAIL;
        }
    }
    else
    {
        // Connect to our address
        err = connect(sock, addr->ai_addr, addr->ai_addrlen);
        if (err == SOCKET_ERROR)
        {
            closesocket(sock);
            freeaddrinfo(addr);
            return ISOCKET_RESULT::CONNECT_FAIL;
        }
    }



    // Should be set up by now!
    out = sock;
    return ISOCKET_RESULT::OK;
}

ISOCKET_RESULT SetSocketNonblocking(SOCKET sock, bool nonblocking)
{
    // Set the client to nonblocking
    u_long nonblock = nonblocking ? 1 : 0;
    int err = ioctlsocket(sock, FIONBIO, &nonblock);
    if (err)
    {
        //closesocket(sock);
        return ISOCKET_RESULT::NONBLOCKING_FAIL;
    }
    return ISOCKET_RESULT::OK;
}



///////////////////////////////
// WinSock TCP Client Socket //
///////////////////////////////
TCPClientSocket::TCPClientSocket() : m_socket(INVALID_SOCKET) {}

// This function is called by the listener socket on client connect
TCPClientSocket::TCPClientSocket(uintptr_t socket) : m_socket(socket) {}

// Shutdown the socket
void TCPClientSocket::Close()
{
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
}

// Connect to a TCP Socket
ISOCKET_RESULT TCPClientSocket::Connect(const char* hostname, const char* servicename)
{
    if (m_socket != INVALID_SOCKET)
        return ISOCKET_RESULT::ALREADYOPEN;
    
    return CreateSocket(m_socket, hostname, servicename, false);
}
#include <stdio.h>
// Non-blocking receive from socket
ISOCKET_RESULT TCPClientSocket::Recv(char* buf, size_t buflen, size_t& bytesWritten)
{
    int nbytes = recv(m_socket, buf, buflen, 0);
	
    if (nbytes < 0)
    {
        bytesWritten = 0;
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return ISOCKET_RESULT::WOULDBLOCK;
        return ISOCKET_RESULT::FAIL;
    }
    
    bytesWritten = nbytes;
    XPRINTF("\tReading %d bytes\n", nbytes);

    return ISOCKET_RESULT::OK;
}

// Non-blocking send from socket
ISOCKET_RESULT TCPClientSocket::Send(char* buf, size_t buflen)
{
    int nbytes = send(m_socket, buf, buflen, 0);

    if (nbytes < 0)
    {
        int err = WSAGetLastError();
        if (err == WSAEWOULDBLOCK)
            return ISOCKET_RESULT::WOULDBLOCK;
        return ISOCKET_RESULT::FAIL;
    }

    //bytesWritten = nbytes;
    XPRINTF("\tSending %d bytes\n", nbytes);


    return ISOCKET_RESULT::OK;
}

ISOCKET_RESULT TCPClientSocket::SetNonblocking(bool nonblocking)
{
    return SetSocketNonblocking(m_socket, nonblocking);
}


///////////////////////////////
// WinSock TCP Listen Socket //
///////////////////////////////
TCPListenSocket::TCPListenSocket() : m_socket(INVALID_SOCKET) {}

void TCPListenSocket::Close()
{
    closesocket(m_socket);
    m_socket = INVALID_SOCKET;
}

// Create TCP listen socket at address
ISOCKET_RESULT TCPListenSocket::Listen(const char* hostname, const char* servicename)
{
    if (m_socket != INVALID_SOCKET)
        return ISOCKET_RESULT::ALREADYOPEN;

    return CreateSocket(m_socket, hostname, servicename, true);
}

// Accept incoming TCP Client
ISOCKET_RESULT TCPListenSocket::Accept(TCPClientSocket& out)
{
    int err;

    // Accept a client socket
    SOCKET client = accept(m_socket, NULL, NULL);
    
    // Do we have to try later?
    err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK)
        return ISOCKET_RESULT::WOULDBLOCK;
    else if (err)
        return ISOCKET_RESULT::ACCEPT_FAIL;

    // Did we actually get anything?
    if (client == INVALID_SOCKET)
        return ISOCKET_RESULT::ACCEPT_FAIL;
    
    // Set the new client as as non-blocking
    u_long nonblock = 1;
    err = ioctlsocket(client, FIONBIO, &nonblock);
    if (err)
    {
        closesocket(client);
        return ISOCKET_RESULT::NONBLOCKING_FAIL;
    }

    // Return our client into the pointer
    out = { client };

    return ISOCKET_RESULT::OK;
}

ISOCKET_RESULT TCPListenSocket::SetNonblocking(bool nonblocking)
{
    return SetSocketNonblocking(m_socket, nonblocking);
}