#pragma once


// Generic results for use by all sockets
enum class ISOCKET_RESULT
{
	OK = 0,
	
	WOULDBLOCK,

	ALREADYOPEN,

	// All fail states below
	FAIL,
	STARTUP_FAIL,
	CREATE_FAIL,
	CONNECT_FAIL,
	SEND_FAIL,
	RESOLVE_ADDRESS_FAIL,
	BIND_FAIL,
	LISTEN_FAIL,
	ACCEPT_FAIL,
	NONBLOCKING_FAIL,
};


// TODO: Eventually, these should be made to take in a protocol parameter or something similar
ISOCKET_RESULT SystemInitSocket();
ISOCKET_RESULT SystemShutdownSocket();

class IListenSocket
{

};

class IClientSocket
{

};