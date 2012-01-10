cmake_minimum_required (VERSION 2.8.6)
project(butcher)

set(CMAKE_C_FLAGS "-std=gnu99 -Wall -Wextra -pedantic -g -ggdb -D_GNU_SOURCE")
set(CMAKE_SHARED_LIBRARY_C_FLAGS "-Wl,--no-undefined")
#-Werror -fprofile-arcs -ftest-coverage

include_directories(${butcher_SOURCE_DIR})

add_executable(bexec
  ${butcher_SOURCE_DIR}/bexec.c
)
set_target_properties(bexec PROPERTIES COMPILE_FLAGS "-rdynamic -pthread")
set_target_properties(bexec PROPERTIES LINK_FLAGS "-Wl,-rpath,.  -rdynamic -pthread")
target_link_libraries(bexec dl)

add_executable(butcher
  ${butcher_SOURCE_DIR}/butcher.c
  ${butcher_SOURCE_DIR}/bt.c
)
set_target_properties(butcher PROPERTIES COMPILE_FLAGS "-rdynamic -pthread")
set_target_properties(butcher PROPERTIES LINK_FLAGS "-Wl,-rpath,.  -rdynamic -pthread")
target_link_libraries(butcher dl elf)

add_library(foo SHARED
  ${butcher_SOURCE_DIR}/libfoo.c
)
set_target_properties(foo PROPERTIES COMPILE_FLAGS "-fPIC")

