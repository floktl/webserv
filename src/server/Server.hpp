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
		void parseAccessRights(Context& ctx);
		bool checkAccessRights(Context &ctx, std::string errorString);
		bool sendWrapper(Context& ctx, std::string http_response);
		bool handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs);

	private:
		GlobalFDS& globalFDS;
		std::vector<ServerBlock> configData;
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
		void determineType(Context& ctx, std::vector<ServerBlock>& configs);
		bool matchLoc(Location& loc, std::string rawPath);
		std::string normalizePath(const std::string& raw);
		bool isMethodAllowed(Context& ctx);
		std::string concatenatePath(const std::string& root, const std::string& path);
		std::string getDirectory(const std::string& path);
		bool buildAutoIndexResponse(Context& ctx, std::stringstream* response);
		bool handleRead(Context& ctx, std::vector<ServerBlock> &configs);
		bool parseHeaders(Context& ctx);
		bool handleWrite(Context& ctx);
		bool queueResponse(Context& ctx, const std::string& response);
		bool isRequestComplete(const Context& ctx);
		std::string approveExtention(Context& ctx, std::string path_to_check);
};

#endif

// void Server::buildStaticResponse(Context &ctx) {
// 	bool keepAlive = false;
// 	auto it = ctx.headers.find("Connection");
// 	if (it != ctx.headers.end()) {
// 		keepAlive = (it->second.find("keep-alive") != std::string::npos);
// 	}

// 	std::stringstream content;
// 	content << "epoll_fd: " << ctx.epoll_fd << "\n"
// 			<< "client_fd: " << ctx.client_fd << "\n"
// 			<< "server_fd: " << ctx.server_fd << "\n"
// 			<< "type: " << static_cast<int>(ctx.type) << "\n"
// 			<< "method: " << ctx.method << "\n"
// 			<< "path: " << ctx.path << "\n"
// 			<< "version: " << ctx.version << "\n"
// 			<< "body: " << ctx.body << "\n"
// 			<< "location_path: " << ctx.location_path << "\n"
// 			<< "requested_path: " << ctx.requested_path << "\n"
// 			<< "error_code: " << ctx.error_code << "\n"
// 			<< "port: " << ctx.port << "\n"
// 			<< "name: " << ctx.name << "\n"
// 			<< "root: " << ctx.root << "\n"
// 			<< "index: " << ctx.index << "\n"
// 			<< "client_max_body_size: " << ctx.client_max_body_size << "\n"
// 			<< "timeout: " << ctx.timeout << "\n";

// 	content << "\nHeaders:\n";
// 	for (const auto& header : ctx.headers) {
// 		content << header.first << ": " << header.second << "\n";
// 	}

// 	content << "\nError Pages:\n";
// 	for (const auto& error : ctx.errorPages) {
// 		content << error.first << ": " << error.second << "\n";
// 	}

// 	std::string content_str = content.str();

// 	std::stringstream http_response;
// 	http_response << "HTTP/1.1 200 OK\r\n"
// 			<< "Content-Type: text/plain\r\n"
// 			<< "Content-Length: " << content_str.length() << "\r\n"
// 			<< "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n"
// 			<< "\r\n"
// 			<< content_str;

// 	sendWrapper(ctx,  http_response.str());
// }


// bool Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev) {
//     int client_fd = globalFDS.clFD_to_svFD_map[fd];
//     RequestBody &req = globalFDS.context_map[client_fd];
// 	// Only process method check once at start
//     if (!req.cgiMethodChecked) {
// 		req.cgiMethodChecked = true;
//         if (!clientHandler->processMethod(req, epoll_fd))
// 		{
//             return true;
// 		}
//     }
//     if (ev & EPOLLIN)
//         cgiHandler->handleCGIRead(epoll_fd, fd);
//     if (fd == req.cgi_out_fd && (ev & EPOLLHUP)) {
//         if (!req.cgi_output_buffer.empty())
//             cgiHandler->finalizeCgiResponse(req, epoll_fd, client_fd);
//         cgiHandler->cleanupCGI(req);
//     }
//     if (fd == req.cgi_in_fd) {
//         if (ev & EPOLLOUT)
//             cgiHandler->handleCGIWrite(epoll_fd, fd, ev);
//         if (ev & (EPOLLHUP | EPOLLERR)) {
//             epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
//             close(fd);
//             req.cgi_in_fd = -1;
//             globalFDS.clFD_to_svFD_map.erase(fd);
//         }
//     }
//     return true;
// }




//bool Server::staticHandler(Context& ctx, int epoll_fd, int fd, uint32_t ev)
//{
//    RequestBody &req = globalFDS.context_map[fd];

//    if (ev & EPOLLIN)
//    {
//        // clientHandler->handleClientRead(epoll_fd, fd);
//        // //Logger::file("handleClient");
//    }

//    if (req.state != RequestBody::STATE_CGI_RUNNING &&
//        req.state != RequestBody::STATE_PREPARE_CGI &&
//        (ev & EPOLLOUT))
//    {
//        // clientHandler->handleClientWrite(epoll_fd, fd);
//    }

//    // Handle request body
//    if ((req.state == RequestBody::STATE_CGI_RUNNING ||
//        req.state == RequestBody::STATE_PREPARE_CGI) &&
//        (ev & EPOLLOUT) && !req.response_buffer.empty())
//    {
//        char send_buffer[8192];
//        size_t chunk_size = std::min(req.response_buffer.size(), sizeof(send_buffer));

//        std::copy(req.response_buffer.begin(),
//				req.response_buffer.begin() + chunk_size,
//				send_buffer);

//        ssize_t written = write(fd, send_buffer, chunk_size);

//        if (written > 0)
//        {
//            req.response_buffer.erase(
//                req.response_buffer.begin(),
//                req.response_buffer.begin() + written
//            );

//            if (req.response_buffer.empty())
//            {
//                delFromEpoll(epoll_fd, fd);
//            }
//        }
//        else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
//        {
//            delFromEpoll(epoll_fd, fd);
//        }
//    }

//    return true;
//}

