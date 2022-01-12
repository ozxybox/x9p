# CMakeList.txt : CMake project for Calcifer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set(SOURCES
	main.cpp

	shared/ISocket.h
	shared/TCPSocket.h
	shared/TCPSocket.cpp
	shared/TCPSocket_windows.cpp
	
	"shared/x9p/X9P.h"
	"shared/x9p/X9PMessages.h"
	"shared/x9p/X9PServer.h"
	"shared/x9p/X9PServer.cpp"
	"shared/x9p/X9PConnection.h"
	"shared/x9p/X9PConnection.cpp"
	"shared/x9p/X9PFid.h"
	"shared/x9p/fs.h"
	"shared/x9p/fs.cpp"
	"shared/x9p/fs_virtual.cpp"
	"shared/x9p/xstring.h"
	"shared/x9p/xstring.cpp"
	"shared/x9p/X9PFid.cpp"
	"shared/x9p/permissions.h"
	"shared/x9p/permissions.cpp"
)

set(INCLUDE_DIRS
	"shared/"
	"shared/x9p/"
)

# Add source to this project's executable.
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
add_executable( Calcifer ${SOURCES} )
target_include_directories( Calcifer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${INCLUDE_DIRS})

