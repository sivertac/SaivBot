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
//#include "../include/DankHttp.hpp"

int main(int argc, char** argv)
{
	boost::asio::io_context ioc;
	std::filesystem::path config_path("Config.txt");
	SaivBot client(ioc, config_path);
	client.run();
	


	//std::make_shared<DankHttp::NuulsUploader>(ioc)->run(
	//	[](auto && str) {std::cout << str << "\n"; }, 
	//	"This is the first line.\n This is the second line\n This is the third line\n", 
	//	"i.nuuls.com", 
	//	"443", 
	//	"/upload"
	//);

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
