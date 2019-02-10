cmake_minimum_required(VERSION 3.13.0)
project (SaivBot)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

if (UNIX)
	set(Openssl_LIBRARIES 
		ssl
		crypto
	)
endif (UNIX)
if (MSVC)
	set(Openssl_LIBRARIES 
		libcrypto_static.lib 
		libssl_static.lib
	)
endif (MSVC)

add_executable(SaivBot
	${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/SaivBot.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/DankHttp.cpp	
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessage.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessageBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/IRCMessageTimedBuffer.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/Log.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/src/LogDownloader.cpp
)	

if (CMAKE_BUILD_TYPE EQUAL "DEBUG") 
	message("Debug build")
	target_compile_definitions(SaivBot PRIVATE SaivBot_TESTMODE)
else (CMAKE_BUILD_TYPE EQUAL "DEBUG")
	message("Release build")	
endif (CMAKE_BUILD_TYPE EQUAL "DEBUG")

target_include_directories(SaivBot PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(SaivBot PRIVATE ${Date_INCLUDE_DIRS})
target_include_directories(SaivBot PRIVATE ${Json_INCLUDE_DIRS})
target_include_directories(SaivBot PRIVATE ${OptionParser_INCLUDE_DIRS})
target_include_directories(SaivBot PRIVATE ${Boost_INCLUDE_DIRS})
target_include_directories(SaivBot PRIVATE ${Openssl_INCLUDE_DIRS})
target_link_directories(SaivBot PRIVATE ${Boost_LIBRARY_DIRS})
target_link_directories(SaivBot PRIVATE ${Openssl_LIBRARY_DIRS})
target_link_libraries(SaivBot PRIVATE ${Boost_LIBRARIES})
target_link_libraries(SaivBot PRIVATE ${Openssl_LIBRARIES})

if (UNIX)
	target_link_libraries(SaivBot PRIVATE stdc++fs pthread)
endif (UNIX)

if (NOT DEFINED Date_INCLUDE_DIRS)
	message("Date_INCLUDE_DIRS not defined")
endif (NOT DEFINED Date_INCLUDE_DIRS)

if (NOT DEFINED Json_INCLUDE_DIRS)
	message("Json_INCLUDE_DIRS not defined")
endif (NOT DEFINED Json_INCLUDE_DIRS)

if (NOT DEFINED OptionParser_INCLUDE_DIRS)
	message("OptionParser_INCLUDE_DIRS not defined")
endif (NOT DEFINED OptionParser_INCLUDE_DIRS)

if (NOT DEFINED Boost_INCLUDE_DIRS)
    message("Boost_INCLUDE_DIRS not defined")
endif (NOT DEFINED Boost_INCLUDE_DIRS)

if (NOT DEFINED Boost_LIBRARY_DIRS)
    message("Boost_LIBRARY_DIRS not defined")
endif (NOT DEFINED Boost_LIBRARY_DIRS)

if (NOT DEFINED Openssl_INCLUDE_DIRS)
    message("Openssl_INCLUDE_DIRS not defined")
endif (NOT DEFINED Openssl_INCLUDE_DIRS)

if (NOT DEFINED Openssl_LIBRARY_DIRS)
    message("Openssl_LIBRARY_DIRS not defined")
endif (NOT DEFINED Openssl_LIBRARY_DIRS)

#target_link_libraries(SaivBot ${CMAKE_THREAD_LIBS_INIT})
#target_link_libraries(SaivBot ${Boost_LIBRARIES})	
#target_link_libraries(SaivBot -lstdc++fs)
#target_link_libraries(SaivBot ssl crypto)



