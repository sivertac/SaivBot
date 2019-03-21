//main.cpp
//Author: Sivert Andresen Cubedo

#if 0
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

#if 1
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
		Log::log_identifier id("pajlada", TimeDetail::TimePeriod(std::chrono::system_clock::now(), std::chrono::system_clock::now() + std::chrono::seconds(10)), "saivnator");
		std::ifstream ifs("test_log.txt", std::ios::binary);
		std::vector<char> data;
		ifs.seekg(0, std::ios::end);
		data.reserve(ifs.tellg());
		ifs.seekg(0, std::ios::beg);
		data.assign((std::istreambuf_iterator<char>(ifs)),
			std::istreambuf_iterator<char>());

		//log1.emplace(TimeDetail::TimePeriod(std::chrono::system_clock::now(), std::chrono::system_clock::now() + std::chrono::seconds(10)), "name", std::move(data), gempirLogParser);
		log1 = gempir_log_parser(std::move(id), std::move(data));
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
		else {
			std::cout << "log2 failed\n";
		}
	}
	

	return 0;
}

#endif

#if 0
//C++
#include <iostream>
#include <string>
#include <thread>

//Local
#include "../include/lru_cache.hpp"
#include "../include/log_cache.hpp"

int main(int argc, char** argv)
{

	lru_cache<int, int> cache(1);

	cache.put(1, 10);
	cache.put(2, 20);
	cache.put(3, 30);
	cache.put(4, 40);
	cache.put(1, 11);
	cache.put(5, 111);
	cache.put(6, 1111);
	cache.put(7, 11111);

	cache.get(1);

	cache.resize(5);

	cache.put(10, 123);
	cache.put(11, 321);

	cache.remove_size_limit();

	cache.put(1, 10);
	cache.put(2, 20);
	cache.put(3, 30);
	cache.put(4, 40);
	cache.put(1, 11);
	cache.put(5, 111);
	cache.put(6, 1111);
	cache.put(7, 11111);

	cache.resize(100);

	cache.put(111, 10);
	cache.put(112, 20);
	cache.put(113, 30);
	cache.put(114, 40);
	cache.put(111, 11);
	cache.put(115, 111);
	cache.put(116, 1111);
	cache.put(117, 11111);

	cache.resize(2);

	for (auto & pair : cache.get_queue()) {
		std::cout << pair.second << ", ";
	}
	std::cout << "\n";
	std::cout << "map size: " << cache.get_map().size() << "\n";



	return 0;
}

#endif

#if 0
//C++
#include <iostream>
#include <string>
#include <thread>
#include <filesystem>

//Local
#include "../include/log_cache.hpp"

int main(int argc, char** argv)
{
	std::filesystem::path cache_dir(".");

	log_cache cache(std::move(cache_dir));

	std::cout << sizeof(std::filesystem::path) << "\n";

	return 0;
}

#endif
