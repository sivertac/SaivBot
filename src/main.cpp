//main.cpp
//Author: Sivert Andresen Cubedo

#if 1
//C++
#include <iostream>
#include <string>

//Boost
#include <boost\asio.hpp>

//Local
#include "../include/SaivBot.hpp"

int main(int argc, char** argv)
{
	boost::asio::io_context ioc;
	
	std::filesystem::path config_path("Config.txt");
	SaivBot client(ioc, config_path);
	client.run();

	while (true) {
		try {
			ioc.run();
			break;
		}
		catch (std::exception & e) {
			std::cerr << "Exception thrown: " << e.what() << "\n";
		}
	}


	return 0;
}

#endif
