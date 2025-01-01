#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "../main.hpp"

class Server;

class StaticHandler {
	private:
		struct FileState {
			std::ifstream file;
			size_t remaining_bytes;
			bool headers_sent;
		};
		std::map<int, FileState> file_states;
		Server& server;

	public:
		StaticHandler(Server& server);
		void handleClientRead(int epfd, int fd);
		void handleClientWrite(int epfd, int fd);
};

#endif