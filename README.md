# SaivBot
A twitch-irc bot implemented in C++17.
The main feature of SaivBot is to lookup data in twitch irc logs. This is done by a user sending a query to the bot (via twitch chat), then the bot will parse the query (using [OptionParser](https://github.com/SaivNator/OptionParser)) and compile a list of relevant logs.
The logs will then be downloaded from a 3rd party twitch chat log host ([justlog](https://api.gempir.com) and [OverRustleLogs](https://overrustlelogs.net)) are currently supported) and will be parsed according to the query.
Depending on the type of query, the result will either be sent back directly to the user in twitch chat or the bot will upload the result to a 3rd party hosting service ([nuuls.com](https://nuuls.com/i) is currently supported) and a link will be sent to the user in chat.

## Commands
|Command|Arguments|Optional flags|Description|
|-|-|-|-|
|shutdown|||Orderly shut down bot.|
|help|command||Get info about command.|
|count|target|-channel -user -period -caseless -service -regex|Count the occurrences of target in logs.|
|find|target|-channel -user -period -caseless -service -regex|Find all lines containing target in logs.|
|clip||-lines_from_now|Capture a snapshot of chat.|
|promote|user||Whitelist user.|
|demote|user||Remove user from whitelist.|
|join|channel||Join channel.|
|part|channel||Part channel.|
|uptime|||Get uptime.|
|say|string|pajaSx|Make bot say something.|
|ping|||Ping the bot.|
|commands|||Get link to commands doc.|
|flags|||Get link to flags doc.|

## Flags
|Flag|Arguments|Description|
|-|-|-|
|-channel|channel|Specify channel in query.|
|-user|user|Specify user in query.|
|-period|start end|Specify time period in query, time points are parsed as "%Y-%m-%d-%H-%M-%S", it is possible to omit parts of the time point right to left. Example: "2018" and "2018-1-1-0-0-0" are parsed as the same.|
|-caseless||Specify that search target is caseless.|
|-regex||Specify that search target is a regex string.|
|-lines_from_now|number|Specify how many lines should be clipped from "now".|

## Dependencies
* C++17
* Boost 1.69.0 Asio
* Boost 1.69.0 Beast
* OpenSSL
* Date https://github.com/HowardHinnant/date
* OptionParser https://github.com/SaivNator/OptionParser
* json https://github.com/nlohmann/json

## Build
You will need cmake 3.13.0 or later and a C++17 compatible compiler.


1. Download/compile/git clone dependencies listed above
2. Git clone https://github.com/SaivNator/SaivBot.git
3. Run cmake command with dependency locations defined

cmake example
    
    cmake ../SaivBot -DCMAKE_CXX_COMPILER=/usr/bin/g++-8 -DJson_INCLUDE_DIRS=../json/include -DDate_INCLUDE_DIRS=../date/include -DOptionParser_INCLUDE_DIRS=../OptionParser/include/ -DBoost_INCLUDE_DIRS=../boost_1_69_0 -DBoost_LIBRARY_DIRS=../boost_1_69_0 -DOpenssl_INCLUDE_DIRS=../openssl-1.1.1a/include/ -DOpenssl_LIBRARY_DIRS=../openssl-1.1.1a

4. Build the project

Linux: 
    
    make
    
Window: 
    
    cmake --build . --config Release
    cmake --build . --config Debug 

## Run
Run SaivBot, the program should create a file "Confix.txt" in the same directory as the SaivBot binary then terminate.
The config file uses json, fill in:
* nick - username of twitch account
* password - oauth token of twitch account, can be generated with http://www.twitchapps.com/tmi/
* channels - list of channels the bot should join when starting up
* host - domain of the twitch-irc server, usually "irc.chat.twitch.tv"
* port - connect port (must be ssl), usually "6697"
* modlist - list of users that have moderator access

Then run SaivBot again, SaivBot should connect to twitch irc.
