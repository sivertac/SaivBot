cmake_minimum_required(VERSION 3.10.1)
project (SaivBot)

set(CMAKE_CXX_COMPILER g++)

#set(BOOST_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../boost)
set(Boost_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../boost/boost_1_67_0)
set(Boost_LIBRARY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../boost/boost_1_67_0/stage/lib)
#set(OPENSSL_ROOT_DIR /usr/local/ssl)
#set(DOPENSSL_LIBRARIES /usr/local/ssl/lib)

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.67.0 REQUIRED COMPONENTS system) 
include_directories(${Boost_INCLUDE_DIRS})    
find_package(OpenSSL REQUIRED)
find_package(Threads)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

set(SaivBot 
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaivBot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/DankHttp.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessageBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessageTimedBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LogDownloader.cpp
	)

if (Boost_FOUND)
	include_directories(${Boost_INCLUDE_DIRS})
	add_executable(SaivBot ${SaivBot})
	target_compile_options(SaivBot PRIVATE -std=c++17 -O3)
	target_link_libraries(SaivBot ${CMAKE_THREAD_LIBS_INIT})
	target_link_libraries(SaivBot ${Boost_LIBRARIES})	
	target_link_libraries(SaivBot -lstdc++fs)
	target_link_libraries(SaivBot ssl crypto)

	target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../date/include)
	target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../json/include)
	target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../OptionParser/include)
endif()

#cmake <source dir> -DCMAKE_TOOLCHAIN_FILE=[vcpkg root]\scripts\buildsystems\vcpkg.cmake
