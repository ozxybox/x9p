#include "TCPSocket.h"
#include "X9PServer.h"
#include <iostream>
#include <chrono>
#include <thread>

void runcl();
void runsv();

int main(int argc, const char** args)
{
	/*
	if (argc < 2)
	{
		printf("Missing arguments! main.exe [s, c]\n");
		return 0;
	}
	*/

	SystemInitSocket();
	//if (strncmp(args[1], "s", 1) == 0)
	{
		// Server Mode
		printf("Hosting server on localhost:27015\n\n");
		runsv();
	}
	/*
	else if (strncmp(args[1], "c", 1) == 0)
	{
		// Client Mode
		printf("Client connecting to localhost:27015\n\n");
		runcl();
	}
	*/


	return 0;
}


void runsv()
{
	CBaseX9PServer sv;
	sv.Begin(nullptr, "27015");

	while (1)
	{
		//printf(".");
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		
		sv.AcceptConnections();
		sv.ProcessPackets();
	}
}

void runlisten()
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
			for (int k = 0; k < out; k++)
			{
				printf("%d|", (int)buf[k]);
			}

		} while (out);
		printf("Client Disconnected.\n");
	}
	
}


TCPClientSocket client;

void sendit(char* buf, size_t len)
{
	ISOCKET_RESULT r = ISOCKET_RESULT::WOULDBLOCK;
	
sendagain:
	r = client.Send(buf, len);
	if (r == ISOCKET_RESULT::WOULDBLOCK)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(200));
		goto sendagain;
	}
}

#include <vector>
#include <unordered_map>


struct strview_t
{
	const char* str;
	int len;
};


struct clcommand_t
{
	const char* name;
	void(*m_func)();
};

void mkdir()
{
	sendit("mkdir", 6);
}

std::vector<clcommand_t> s_commands = {

	{"mkdir", mkdir},

	{"ls", mkdir},

};


void interpret(const char* str)
{
	std::vector<strview_t> toks;
	// Tokenize the string
	for (const char* c = str; *c; c++)
	{
		// Skip whitespaces
		for (; isspace(*c) && *c; c++);
		if (!*c) break;

		const char* start = c;

		// Are we in a quote?
		if (*c == '"')
		{
			c++; // Skip the quote

			for (;; c++)
			{
				// Only break if we hit another quote without an escape character behind it
				char k = *c;
				if (k == '\0')
					break; // End of string. Bit unexpected eh?
				else if (k == '\\')
				{
					// Escape sequence skips char
					c++;
					if (*c == '\0')
						break;
				}
				else if(k == '"')
					break; // End of quote
			}

			// Cool, this is a whole token.
			strview_t s = { start, c - start };
			toks.push_back(s);
			continue;
		}


		
		// Split on the spaces
		for (;; c++)
		{
			char k = *c;
			
			// End of string
			if (k == '\0') break;
			
			// End of token
			if (k == ' ' || k == '\t') break;

			// End of line
			if (k == '\n' || k == '\r') break;
		}
		strview_t s = { start, c - start };
		toks.push_back(s);

		// End of line end of loop
		if (*c != ' ' && *c != '\t') break;

	}

	// Did we get anything?
	if (!toks.size())
		return;

	// We've got a big pool of tokens. Now work on it

	for (int i = 0; i < s_commands.size(); i++)
	{
		if (strncmp(toks[0].str, s_commands[i].name, toks[0].len) == 0)
		{
			s_commands[i].m_func();
		}

	}
}

void runcl()
{
	ISOCKET_RESULT r = client.Connect("127.0.0.1", "27015");
	if (r != ISOCKET_RESULT::OK)
	{
		printf("FAILED TO CONNECT!");
		return;
	}

	printf("Connected!\n");

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	
	while (true)
	{
		putchar('>');
		char input[512];
		fgets(input, 512, stdin);

		interpret(input);
	}
	

	

}
