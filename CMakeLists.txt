cmake_minimum_required (VERSION 2.8)
project (httpsrv)

add_executable (final httpsrv.c)
target_link_libraries(final rt pthread)