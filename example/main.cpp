#include "X9PServer.h"
#include "X9PClient.h"
#include "socket/TCPSocket.h"
#include <iostream>
#include <chrono>
#include <thread>
#include "xstring.h"
#include "fs.h"

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

	X9PServer sv;
	sv.Begin(nullptr, "27015", &vfs);

	while (1)
	{
		//printf(".");
		std::this_thread::sleep_for(std::chrono::milliseconds(2));

		sv.AcceptConnections();
		sv.ProcessPackets();
	}
}



void runcl()
{
	X9PClient cl;
	xerr_t err = cl.Begin("127.0.0.1", "27015");

	if (err) printf(err);

	xstr_t uname = xstrdup("user");
	xstr_t aname = xstrdup("/");
	cl.Tattach(0, NOFID, uname, aname, [&](xerr_t err, qid_t* qid) {
		printf("%s\n", err ? err : "ATTACH");
	});
	cl.Twalk(0, 1, 1, xstrdup("hello.txt"), [&](xerr_t err, uint16_t nwqid, qid_t* wqid) {
		printf("%s\n", err ? err : "WALK");
	});
	cl.Topen(1, X9P_OPEN_READ, [&](xerr_t err, qid_t* qid, uint32_t iounit) {
		printf("%s\n", err ? err : "OPEN");
	});
	cl.Tread(1, 0, 512, [&](xerr_t err, uint32_t count, uint8_t* data) {
		printf("%s\n", err ? err : "READ");
		fwrite(data, 1, count, stdout);
	});
	cl.Twrite(1, 0, 5, (uint8_t*)"birds", [&](xerr_t err, uint32_t count) {
		printf("%s\n", err ? err : "WRITE");
	});
	cl.Tread(1, 0, 512, [&](xerr_t err, uint32_t count, uint8_t* data) {
		printf("%s\n", err ? err : "READ");
		fwrite(data, 1, count, stdout);
	});


	while (1)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2));

		cl.ProcessPackets();
	}
}




/*
file resource fs

map.vmf/
       /brushes/
	           /0
			   /1
			   /2
			   /3
	   /entities/

image.png/
		 /width
		 /height
		 /format
         /pixels


struct image
{
	ui32 width;
	ui32 height;
	int format;
	char* pixels;
}

res = expose_resource(IMAGE, "dog.png", filedata);
res.field("width",  &image.width,  sizeof(ui32), READ_ONLY);
res.field("height", &image.width,  sizeof(ui32), READ_ONLY);
res.field("format", &image.format, sizeof(int),  READ_ONLY);
res.field("pixels", image.pixels,  width*height*fmt.size(), READ_ONLY);

*/