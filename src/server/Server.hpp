#ifndef SERVER_HPP
#define SERVER_HPP

#include "../structs/webserv.hpp"
#include "../config/ConfigHandler.hpp"
#include "../utils/Logger.hpp"
#include "../cgi/CgiHandler.hpp"
# include "../utils/Sanitizer.hpp"
# include "../error/ErrorHandler.hpp"
# include "../server/Server.hpp"
# include "../structs/webserv.hpp"

extern std::unique_ptr<Server> serverInstance;

struct GlobalFDS;
struct ServerBlock;

class CgiHandler;
class ErrorHandler;

class Server
{
	public:

		// server_init
		Server(GlobalFDS &_globalFDS);
		~Server();

		int server_init(std::vector<ServerBlock> configs);
		GlobalFDS& getGlobalFds(void);
		ErrorHandler* getErrorHandler(void);
		int has_gate = false;
		char **environment;

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
		bool checkAccessRights(Context &ctx, std::string path);
		bool fileReadable(const std::string& path);
		bool fileExecutable(const std::string& path);
		bool dirReadable(const std::string& path);
		bool dirWritable(const std::string& path);
		bool sendHandler(Context& ctx, std::string http_response);
		bool handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs);

	private:
		GlobalFDS& globalFDS;
		std::vector<ServerBlock> configData;
		ErrorHandler* errorHandler;
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
		bool handleCGIEvent(int epoll_fd, int fd, uint32_t ev);
		void buildStaticResponse(Context &ctx);

		// Server loop
		bool dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs);
		bool acceptNewConnection(int epoll_fd, int server_fd, std::vector<ServerBlock> &configs);
		void checkAndCleanupTimeouts();
		void killTimeoutedCGI(RequestBody &req);

		void logContext(const Context& ctx, const std::string& event = "");
		void parseRequest(Context& ctx);
		std::string requestTypeToString(RequestType type);
		bool determineType(Context& ctx, std::vector<ServerBlock> configs);
		bool matchLoc(const std::vector<Location>& locations, const std::string& rawPath, Location& bestMatch);
		std::string normalizePath(const std::string& raw);
		std::string concatenatePath(const std::string& root, const std::string& path);
		std::string subtractLocationPath(const std::string& path, const Location& location);
		std::string getDirectory(const std::string& path);
		bool buildAutoIndexResponse(Context& ctx, std::stringstream* response);
		bool parseHeaders(Context& ctx, const std::vector<ServerBlock>& configs);
		bool handleWrite(Context& ctx);
		bool queueResponse(Context& ctx, const std::string& response);
		bool isRequestComplete(Context& ctx);
		std::string approveExtention(Context& ctx, std::string path_to_check);
		void parseChunkedBody(Context& ctx);
		bool redirectAction(Context& ctx);
		bool deleteHandler(Context &ctx);
		void getMaxBodySizeFromConfig(Context& ctx, std::vector<ServerBlock> configs);
		bool resetContext(Context& ctx);
		bool handleContentLength(Context& ctx, const std::vector<ServerBlock>& configs);
		bool processParsingBody(Context& ctx);
		bool handleParsingPhase(Context& ctx, const std::vector<ServerBlock>& configs);
		bool finalizeRequest(Context& ctx);
		bool handleRead(Context& ctx, std::vector<ServerBlock>& configs);


		void parseCookies(Context& ctx, std::string value);
		std::string generateSetCookieHeader(const Cookie& cookie);
		void parseMultipartHeaders(Context& ctx);


		std::vector<DirEntry> getDirectoryEntries(Context& ctx);
		void generateAutoIndexBody(const std::vector<DirEntry>& entries, std::stringstream& content);
		void generateAutoIndexHeader(Context& ctx, std::stringstream& content);
		bool parseHeaderFields(Context& ctx, std::istringstream& stream);
		bool parseRequestLine(Context& ctx, std::istringstream& stream);
		std::vector<std::string> splitPathLoc(const std::string& path);
		std::vector<std::string> getBlocksLocsPath(const std::vector<Location>& locations);
		void prepareUploadPingPong(Context& ctx);
		void handleSessionCookies(Context& ctx);
		std::string retreiveReqRoot(Context &ctx);
		bool isMultipartUpload(Context& ctx);
		bool prepareMultipartUpload(Context& ctx, std::vector<ServerBlock> configs);
		bool doMultipartWriting(Context& ctx);
		bool completeUpload(Context& ctx);
		void initializeWritingActions(Context& ctx);
		size_t findBodyStart(const std::string& buffer, Context& ctx);
		std::string extractBodyContent(const char* buffer, ssize_t bytes, Context& ctx);
		bool readingTheBody(Context& ctx, const char* buffer, ssize_t bytes);
		bool extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx);
};

std::string extractHostname(const std::string& header);
void printServerBlock(ServerBlock& serverBlock);
void printRequestBody(const RequestBody& req);
std::string getEventDescription(uint32_t ev);
std::string trim(const std::string& str);
std::vector<std::string> parseOptionsToVector(const std::string& opts);
std::string expandEnvironmentVariables(const std::string& value, char** env);
void log_global_fds(const GlobalFDS& fds);
void log_server_configs(const std::vector<ServerBlock>& configs);
bool	updateErrorStatus(Context &ctx, int error_code, std::string error_string);
int extractPort(const std::string& header);

#endif
