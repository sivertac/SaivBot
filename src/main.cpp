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
			std::cout << "[" << std::this_thread::get_id() << "] " << "Exception thrown: " << e.what() << "\n";
		}
	}
}

int main(int argc, char** argv)
{
	boost::asio::io_context ioc;
	std::filesystem::path config_path("Config.txt");
	boost::asio::ssl::context ctx{ boost::asio::ssl::context::sslv23 };
	ctx.set_default_verify_paths();
	SaivBot client(ioc, std::move(ctx), config_path);
	client.run();

	std::ios::sync_with_stdio(true);

	//unsigned int thread_count = 1;
	unsigned int thread_count = std::thread::hardware_concurrency();
	std::cout << "[" << std::this_thread::get_id() << "] " << "Thread count: " << thread_count << "\n";
	if (thread_count > 0) {
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
	else {
		std::cout << "std::this_thread::get_id() error\n";
	}

	std::cout << "[" << std::this_thread::get_id() << "] " << "Mainthread exit\n";



	return 0;
}

#endif
