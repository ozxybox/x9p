
cmake_minimum_required (VERSION 3.0)


add_executable( x9p_example main.cpp )
target_include_directories( x9p_example PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries( x9p_example x9p )

