
cmake_minimum_required (VERSION 3.0)


add_executable( x9p_tests main.cpp )
target_include_directories( x9p_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} )
target_link_libraries( x9p_tests x9p )



add_test(NAME test_x9p_xstrcmp   COMMAND x9p_tests xstrcmp)
add_test(NAME test_x9p_xstrfromd COMMAND x9p_tests xstrfromd)
add_test(NAME test_x9p_xstrtod   COMMAND x9p_tests xstrtod)
add_test(NAME test_x9p_xstrtod_xstrfromd   COMMAND x9p_tests xstrtod_xstrfromd)
