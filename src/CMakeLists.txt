
cmake_minimum_required (VERSION 3.8)

set( X9P_PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../public )

set(SOURCES
	socket/ISocket.h
	socket/TCPSocket.h
	socket/TCPSocket.cpp
	socket/TCPSocket_windows.cpp
	
	"X9P.h"
	"X9PMessages.h"
	"X9PFileSystem.h"
	"X9PServer.h"
	"X9PServer.cpp"
	"X9PClient.h"
	"X9PClient.cpp"
	"fs.h"
	"fs.cpp"
	"fs_virtual.cpp"
	"xstring.h"
	"xstring.cpp"
)


# Add source to this project's executable.
add_library( x9p STATIC ${SOURCES} )
target_include_directories( x9p PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/socket )
target_include_directories( x9p PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${X9P_PUBLIC} )

