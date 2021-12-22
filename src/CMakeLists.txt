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
)


# Add source to this project's executable.
add_executable (Calcifer ${SOURCES})

# TODO: Add tests and install targets if needed.
