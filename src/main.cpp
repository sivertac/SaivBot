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

#if 0

//C++
#include <iostream>
#include <string>
#include <thread>

//Boost
#include <boost/asio.hpp>

//Local
#include "../include/SaivBot.hpp"

int main(int argc, char** argv)
{
	std::optional<Log::Log> log1;
	
	{
		std::ifstream ifs("test_log.txt", std::ios::binary);
		std::vector<char> data;
		ifs.seekg(0, std::ios::end);
		data.reserve(ifs.tellg());
		ifs.seekg(0, std::ios::beg);
		data.assign((std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>());

		log1.emplace(TimeDetail::TimePeriod(std::chrono::system_clock::now(), std::chrono::system_clock::now() + std::chrono::seconds(10)), "name", std::move(data), gempirLogParser);
	}

	{
		std::ofstream ofs("test_log_out.txt", std::ios::binary);
		Log::serializeDefaultLogFormat(ofs, *log1);
	}

	{
		std::ifstream ifs("test_log_out.txt", std::ios::binary);
		auto log2 = Log::deserializeDefaultLogFormat(ifs);
	
		if (log2) {
			std::cout << std::boolalpha;
			std::cout << (*log1 == *log2) << "\n";
		}
	}
	

	return 0;
}

#endif
