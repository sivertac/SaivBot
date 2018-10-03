//main.cpp
//Author: Sivert Andresen Cubedo

#if 1
//C++
#include <iostream>
#include <string>
#include <thread>

//Boost
#include <boost/asio.hpp>

//Local
#include "../include/SaivBot.hpp"

void workFunc(boost::asio::io_context & ioc)
{
	while (true) {
		try {
			ioc.run();
			break;
		}
		catch (std::exception & e) {
			std::cerr << "[" << std::this_thread::get_id() << "] " << "Exception thrown: " << e.what() << "\n";
		}
	}
}

int main(int argc, char** argv)
{
	boost::asio::io_context ioc;
	std::filesystem::path config_path("Config.txt");
	SaivBot client(ioc, config_path);
	client.run();

	unsigned int thread_count = std::thread::hardware_concurrency();
	std::cout << "[" << std::this_thread::get_id() << "] " << "Thread count: " << thread_count << "\n";
	if (thread_count == 0) {
		workFunc(ioc);
	}
	else {
		std::vector<std::thread> threads(thread_count);
		for (auto & t : threads) {
			t = std::thread(workFunc, std::ref(ioc));
		}
		for (auto & t : threads) {
			if (t.joinable()) {
				t.join();
				std::cout << "[" << std::this_thread::get_id() << "] " << "Mainthread join\n";
			}
		}
	}

	std::cout << "[" << std::this_thread::get_id() << "] " << "Mainthread exit\n";


	return 0;
}

#endif

#if 0

#include <iostream>
#include <chrono>
#include "../include/IRCMessage.hpp"

int main(int argc, char** argv)
{

	std::string str1("PING :tmi.twitch.tv");

	IRCMessage irc_msg1(std::chrono::system_clock::now(), std::move(str1));


	irc_msg1.print(std::cout);



	return 0;
}

#endif
