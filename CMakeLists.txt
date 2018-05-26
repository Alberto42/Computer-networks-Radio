cmake_minimum_required(VERSION 3.8)
project(SIK-2)

LINK_DIRECTORIES("/usr/lib/x86_64-linux-gnu/")


set(CMAKE_CXX_STANDARD 17)
set( CMAKE_CXX_FLAGS "-Wall -O2 -pthread" )

set(SOURCE_FILES_NADAJNIK sikradio-sender.cpp err.h err.cpp sikradio-sender.h common.cpp common.h)
set(SOURCE_FILES_ODBIORNIK sikradio-receiver.cpp err.h err.cpp common.cpp common.h gui.h gui.cpp sikradio-receiver.h poll.h poll.cpp)
#set()
add_executable(sikradio-sender ${SOURCE_FILES_NADAJNIK})
add_executable(sikradio-receiver ${SOURCE_FILES_ODBIORNIK})
TARGET_LINK_LIBRARIES(sikradio-sender libboost_program_options.a)
TARGET_LINK_LIBRARIES(sikradio-receiver libboost_program_options.a)
add_executable(regex_test test/regex_test.cpp)
add_executable(test_thread test/test_thread.cpp)
add_executable(test_thread2 test/test_thread2.cpp)