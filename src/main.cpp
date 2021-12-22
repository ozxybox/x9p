#include "shared/TCPSocket.h"
#include <iostream>
#include <chrono>
#include <thread>

void client();
void server();

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
		server();
	}
	else if (strncmp(args[1], "c", 1) == 0)
	{
		// Client Mode
		printf("Client connecting to localhost:27015\n\n");
		client();
	}


	return 0;
}

void client()
{
	TCPClientSocket client;
	ISOCKET_RESULT r = client.Connect("127.0.0.1", "27015");
	if (r != ISOCKET_RESULT::OK)
	{
		printf("FAILED TO CONNECT!");
		return;
	}

	printf("Connected!\n");

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	printf("Sending message");
	r = ISOCKET_RESULT::WOULDBLOCK;
	while (r != ISOCKET_RESULT::OK)
	{
		printf(".");
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		char sendbuf[] = "boogey man\n";
		r = client.Send(sendbuf, sizeof(sendbuf));
	
	}
	printf("\nSent it!\n");
	std::this_thread::sleep_for(std::chrono::milliseconds(20000));

}

void server()
{
	TCPListenSocket listen;
	listen.Listen(nullptr, "27015");


	while (1)
	{
		ISOCKET_RESULT r = ISOCKET_RESULT::WOULDBLOCK;
		TCPClientSocket client;
		printf("Waiting for client");
		while (r == ISOCKET_RESULT::WOULDBLOCK)
		{
			printf(".");
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			r = listen.Accept(client);
		}

		int out = 0;
		do
		{

			printf("\ntrying recv");
			char buf[1024];
			r = ISOCKET_RESULT::WOULDBLOCK;
			while (r == ISOCKET_RESULT::WOULDBLOCK)
			{
				printf(".");
				std::this_thread::sleep_for(std::chrono::milliseconds(200));
				r = client.Recv(buf, 1024 - 1, out);
				buf[out] = 0;
			}
			printf("\nGot!\n");
			printf(buf);
		} while (out);
		printf("Client Disconnected.\n");
	}
	
}