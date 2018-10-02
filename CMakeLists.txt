cmake_minimum_required(VERSION 3.10.1)
project (SaivBot)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SaivBot 
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaivBot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/DankHttp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMEssageBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMEssageTimedBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LogDownloader.cpp
	)

add_executable(SaivBot ${SaivBot})

target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../date/include)
target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../OptionParser/include)

#cmake <source dir> -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]\scripts\buildsystems\vcpkg.cmake