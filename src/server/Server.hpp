#ifndef SERVER_HPP
#define SERVER_HPP

#include "TaskManager.hpp"
#include "../main.hpp"
#include <csignal>

struct GlobalFDS;
struct ServerBlock;

// class ClientHandler;
// class CgiHandler;
// class RequestHandler;
class ErrorHandler;

class Server
{
	public:

		// server_init
		Server(GlobalFDS &_globalFDS);
		~Server();

		int server_init(std::vector<ServerBlock> configs);
		GlobalFDS& getGlobalFds(void);
		// ClientHandler* getClientHandler(void);
		// CgiHandler* getCgiHandler(void);
		// RequestHandler* getRequestHandler(void);
		ErrorHandler* getErrorHandler(void);
		//TaskManager* getTaskManager(void);

		//server_helpers
		void modEpoll(int epfd, int fd, uint32_t events);
		void delFromEpoll(int epfd, int fd);
		int setNonBlocking(int fd);
		enum RequestBody::Task getTaskStatus(int client_fd);
		void setTimeout(int t);
		int getTimeout() const;
		void setTaskStatus(enum RequestBody::Task new_task, int client_fd);
		void cleanup();
		int runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs);
		void logRequestBodyMapFDs();
		void parseAcessRights(Context& ctx);
		void checkAccessRights(Context &ctx, std::string errorString);
		bool sendWrapper(Context& ctx, std::string http_response);
	private:
		GlobalFDS& globalFDS;
		// ClientHandler* clientHandler;
		// CgiHandler* cgiHandler;
		// RequestHandler* requestHandler;
		ErrorHandler* errorHandler;
		//TaskManager* taskManager;
		int timeout;
		std::vector<std::string> added_server_names;

		// server_init
		int initEpoll();
		bool initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs);

		// server_helpers
		bool findServerBlock(const std::vector<ServerBlock> &configs, int fd);
		bool addServerNameToHosts(const std::string &server_name);
		void removeAddedServerNamesFromHosts();

		// server_event_handlers
		bool staticHandler(Context& ctx);
		bool errorsHandler(Context& ctx, uint32_t ev);
		bool handleCGIEvent(int epoll_fd, int fd, uint32_t ev);
		void buildStaticResponse(Context &ctx);

		// Server loop
		bool dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs);
		bool handleNewConnection(int epoll_fd, int fd);
		void checkAndCleanupTimeouts();
		void killTimeoutedCGI(RequestBody &req);

		void logContext(const Context& ctx, const std::string& event = "");
		void parseRequest(Context& ctx);
		std::string requestTypeToString(RequestType type);
		void determineType(Context& ctx, const std::vector<ServerBlock>& configs);
		bool matchLoc(const Location& loc, std::string rawPath);
		std::string normalizePath(const std::string& raw);
		bool isMethodAllowed(Context& ctx);
		std::string concatenatePath(const std::string& root, const std::string& path);
		std::string getDirectory(const std::string& path);
		bool buildAutoIndexResponse(Context& ctx, std::stringstream* response);
		bool handleRead(Context& ctx, std::vector<ServerBlock> &configs);
		bool parseHeaders(Context& ctx);
		bool handleWrite(Context& ctx);
		bool queueResponse(Context& ctx, const std::string& response);
		bool processRequest(Context& ctx, std::vector<ServerBlock> &configs);
		bool isRequestComplete(const Context& ctx);
};

#endif
