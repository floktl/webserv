#ifndef SERVER_HPP
#define SERVER_HPP

#include "../main.hpp"

struct GlobalFDS;
struct ServerBlock;

class StaticHandler;
class CgiHandler;
class RequestHandler;
class ErrorHandler;

class Server
{
	public:
		Server(GlobalFDS &_globalFDS);
		~Server();
		int server_init(std::vector<ServerBlock> configs);
		void delFromEpoll(int epfd, int fd);
		void modEpoll(int epfd, int fd, uint32_t events);
		GlobalFDS& getGlobalFds(void);
		StaticHandler* getStaticHandler(void);
		CgiHandler* getCgiHandler(void);
		RequestHandler* getRequestHandler(void);
		ErrorHandler* getErrorHandler(void);
		int setNonBlocking(int fd);

	private:
		GlobalFDS& globalFDS;
		StaticHandler* staticHandler;
		CgiHandler* cgiHandler;
		RequestHandler* requestHandler;
		ErrorHandler* errorHandler;
		int initializeEpoll();
		int setupServerSocket(ServerBlock& conf);
		void handleCGIEvent(int epoll_fd, int fd, uint32_t ev);
		void handleClientEvent(int epoll_fd, int fd, uint32_t ev, RequestState &req);
		void handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf);
};

#endif
