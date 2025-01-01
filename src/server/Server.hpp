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
		void handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf);
		void finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd);
		int initEpoll();
		bool handleCGIEvent(int epoll_fd, int fd, uint32_t ev);
		bool handleClientEvent(int epoll_fd, int fd, uint32_t ev);
		bool initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs);
		int runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs);
		const ServerBlock* findServerBlock(const std::vector<ServerBlock> &configs, int fd);
		bool dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs);
		bool processMethod(RequestState &req, int epoll_fd);
		bool isMethodAllowed(const RequestState &req, const std::string &method) const;
};

#endif
