#ifndef SERVER_HPP
#define SERVER_HPP

#include "TaskManager.hpp"
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

		// server_init
		Server(GlobalFDS &_globalFDS);
		~Server();
<<<<<<< HEAD

		int server_init(std::vector<ServerBlock> configs);
=======
		int server_loop(std::vector<ServerBlock> configs);
		void delFromEpoll(int epfd, int fd);
		void modEpoll(int epfd, int fd, uint32_t events);
>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
		GlobalFDS& getGlobalFds(void);
		StaticHandler* getStaticHandler(void);
		CgiHandler* getCgiHandler(void);
		RequestHandler* getRequestHandler(void);
		ErrorHandler* getErrorHandler(void);
<<<<<<< HEAD
		TaskManager* getTaskManager(void);

		//server_helpers
		void modEpoll(int epfd, int fd, uint32_t events);
		void delFromEpoll(int epfd, int fd);
=======
		TaskManager& getTaskManager(void);
>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
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
<<<<<<< HEAD
		TaskManager* taskManager;

		// server_init
		int initEpoll();
		bool initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs);

		// server_helpers
		bool isMethodAllowed(const RequestState &req, const std::string &method) const;
		const ServerBlock* findServerBlock(const std::vector<ServerBlock> &configs, int fd);

		// server_event_handlers
		bool handleClientEvent(int epoll_fd, int fd, uint32_t ev);
		bool handleCGIEvent(int epoll_fd, int fd, uint32_t ev);
		void finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd);
		bool processMethod(RequestState &req, int epoll_fd);

		// Server loop
		bool dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs);
		int runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs);
=======
		TaskManager taskManager;

>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
		void handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf);
		void handleCGITimeouts(int epoll_fd, std::chrono::seconds CGI_TIMEOUT);
		int initializeServer(int &epoll_fd, std::vector<ServerBlock> &configs);
};

#endif
