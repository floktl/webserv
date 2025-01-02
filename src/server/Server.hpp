#ifndef SERVER_HPP
#define SERVER_HPP

#include "../main.hpp"

struct GlobalFDS;
struct ServerBlock;

class StaticHandler;
class CgiHandler;
class RequestHandler;
class ErrorHandler;
class TaskManager;

class Server
{
	public:
		Server(GlobalFDS &_globalFDS);
		~Server();
		int server_loop(std::vector<ServerBlock> configs);
		void delFromEpoll(int epfd, int fd);
		void modEpoll(int epfd, int fd, uint32_t events);
		GlobalFDS& getGlobalFds(void);
		StaticHandler* getStaticHandler(void);
		CgiHandler* getCgiHandler(void);
		RequestHandler* getRequestHandler(void);
		ErrorHandler* getErrorHandler(void);
		TaskManager& getTaskManager(void);
		int setNonBlocking(int fd);
		void handleTaskTimeouts();
		std::string handleStartTask();
		std::string handleTaskStatus(const std::string& task_id);

	private:
		GlobalFDS& globalFDS;
		StaticHandler* staticHandler;
		CgiHandler* cgiHandler;
		RequestHandler* requestHandler;
		ErrorHandler* errorHandler;
		TaskManager taskManager;

		void handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf);
		void handleCGITimeouts(int epoll_fd, std::chrono::seconds CGI_TIMEOUT);
		int initializeServer(int &epoll_fd, std::vector<ServerBlock> &configs);
};

#endif
