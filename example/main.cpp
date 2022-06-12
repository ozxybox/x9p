#include "X9PServer.h"
#include "X9PClient.h"
#include "socket/TCPSocket.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "xstring.h"
#include "XVirtualFileSystem.h"
#include "XIO.h"

void runcl();
void runsv();

int main(int argc, const char** args)
{
	
	if (argc < 2)
	{
		printf("Missing arguments! main.exe [s, c]\n");
		return 0;
	}

	SystemInitSocket();
	if (strncmp(args[1], "s", 1) == 0)
	{
		// Server Mode
		printf("Hosting server on localhost:27015\n\n");
		runsv();
	}
	else if (strncmp(args[1], "c", 1) == 0)
	{
		// Client Mode
		printf("Client connecting to localhost:27015\n\n");
		runcl();
	}


	return 0;
}


void runsv()
{
	XVirtualFileSystem vfs;
	vfs.Init();

	X9PServer sv;
	sv.Begin(nullptr, "27015", &vfs);

	float curtime = 0;
	float last_acc = 0;
	while (1)
	{
		//printf(".");
		//std::this_thread::sleep_for(std::chrono::milliseconds(2));


		curtime = clock() / (float)CLOCKS_PER_SEC;
		if (curtime > last_acc + 1.5)
		{
			sv.AcceptConnections();
			last_acc = curtime;
		}
		sv.ProcessPackets();
	}
}


void runcl()
{
	X9PClient cl;
	xerr_t err = cl.Begin("127.0.0.1", "27015");

#if 0
	if (err) printf(err);

	xstr_t uname = XSTR_L("user");
	xstr_t aname = XSTR_L("/");

	XAuth auth;
	auth.client = 0;
	auth.id = 0;
	auth.uname = uname;


	XFile file;

	cl.Tattach(0, NOFID, uname, aname, [&](xerr_t err, qid_t* qid) {
		printf("%s\n", err ? err : "ATTACH");
	});
	cl.Twalk(0, 1, 1, xstrdup("hello.txt"), [&](xerr_t err, uint16_t nwqid, qid_t* wqid) {
		printf("%s\n", err ? err : "WALK");
	});
	cl.Topen(1, X9P_OPEN_READ, [&](xerr_t err, qid_t* qid, uint32_t iounit) {
		printf("%s\n", err ? err : "OPEN");
	});
	cl.Tread(1, 0, 512, [&](xerr_t err, uint32_t count, void* data) {
		printf("%s\n", err ? err : "READ");
		fwrite(data, 1, count, stdout);
	});
	cl.Twrite(1, 0, 5, (void*)"birds", [&](xerr_t err, uint32_t count) {
		printf("%s\n", err ? err : "WRITE");
	});
	cl.Tread(1, 0, 512, [&](xerr_t err, uint32_t count, uint8_t* data) {
		printf("%s\n", err ? err : "READ");
		fwrite(data, 1, count, stdout);
	});


	while (1)
	{
		//std::this_thread::sleep_for(std::chrono::milliseconds(2));

		cl.ProcessPackets();
	}
#endif
}


