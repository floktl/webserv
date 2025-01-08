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

		bool processMethod(RequestState &req, int epoll_fd);
		bool isMethodAllowed(const RequestState &req, const std::string &method) const;
};

#endif