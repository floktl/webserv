#ifndef ClientHandler_HPP
#define ClientHandler_HPP

#include "../main.hpp"

class Server;

class ClientHandler {
	private:
		struct FileState {
			std::ifstream file;
			size_t remaining_bytes;
			bool headers_sent;
		};
		std::map<int, FileState> file_states;
		Server& server;

	public:
		ClientHandler(Server& server);
		void handleClientRead(int epfd, int fd);
		void handleClientWrite(int epfd, int fd);

		bool processMethod(RequestBody &req, int epoll_fd);
		bool isMethodAllowed(const RequestBody &req, const std::string &method) const;
};

#endif