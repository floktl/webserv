#ifndef SERVER_HPP
#define SERVER_HPP

# include "../structs/webserv.hpp"
# include "../config/ConfigHandler.hpp"
# include "../error/ErrorHandler.hpp"
# include "../utils/Logger.hpp"
# include "../utils/Sanitizer.hpp"
# include "../server/Server.hpp"
# include "../structs/webserv.hpp"

struct GlobalFDS;
struct ServerBlock;

class CgiHandler;
class ErrorHandler;

class Server
{
	public:
		char	**environment;
		int		has_gate = false;
		int		&g_shutdown_requested;

		// server_init
		Server(GlobalFDS &_globalFDS, int &_g_shutdown_requested);
		~Server();

		int				server_init(std::vector<ServerBlock> configs);
		GlobalFDS&		getGlobalFds(void);
		ErrorHandler*	getErrorHandler(void);

		void 			removeAddedServerNamesFromHosts();
		int				runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs);
		bool 			sendHandler(Context& ctx, std::string http_response);

	private:
		GlobalFDS&					globalFDS;
		std::vector<ServerBlock>	configData;
		ErrorHandler*				errorHandler;
		int							timeout;
		std::vector<std::string>	added_server_names;
		bool						signal_end;

		// server_helpers
		bool	modEpoll(int epfd, int fd, uint32_t events);
		bool	delFromEpoll(int epfd, int fd);
		int		setNonBlocking(int fd);
		void	setTimeout(int t);
		void	parseAccessRights(Context& ctx);
		bool	checkAccessRights(Context &ctx, std::string path);
		bool	fileReadable(const std::string& path);
		bool	fileExecutable(const std::string& path);
		bool	dirWritable(const std::string& path);
		bool	handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs);
		// serv	_helpers
		bool	findServerBlock(const std::vector<ServerBlock> &configs, int fd);
		bool	addServerNameToHosts(const std::string &server_name);

		// server_event_handlers
		bool	staticHandler(Context& ctx);
		void	buildStaticResponse(Context &ctx);

		// Server Loop
		bool		acceptNewConnection(int epoll_fd, int server_fd, std::vector<ServerBlock> &configs);
		void		checkAndCleanupTimeouts();
		void		clear_global_fd_map(std::chrono::steady_clock::time_point now);

		void		logContext(const Context& ctx, const std::string& event = "");
		std::string	requestTypeToString(RequestType type);
		bool 		determineType(Context& ctx, std::vector<ServerBlock> configs);
		bool 		matchLoc(const std::vector<Location>& locations, const std::string& rawPath, Location& bestMatch);
		std::string	normalizePath(const std::string& raw);
		std::string	concatenatePath(const std::string& root, const std::string& path);
		std::string	subtractLocationPath(const std::string& path, const Location& location);
		std::string	getDirectory(const std::string& path);
		bool		buildAutoIndexResponse(Context& ctx, std::stringstream* response);
		bool		parseBareHeaders(Context& ctx, std::vector<ServerBlock>& configs);
		bool		handleWrite(Context& ctx);
		std::string	approveExtention(Context& ctx, std::string path_to_check);
		bool		redirectAction(Context& ctx);
		bool		deleteHandler(Context &ctx);
		void		getMaxBodySizeFromConfig(Context& ctx, std::vector<ServerBlock> configs);
		bool		resetContext(Context& ctx);
		bool		checkMaxContentLength(Context& ctx, std::vector<ServerBlock>& configs);
		bool		handleRead(Context& ctx, std::vector<ServerBlock>& configs);


		void parseCookies(Context& ctx, std::string value);
		std::string generateSetCookieHeader(const Cookie& cookie);
		void parseMultipartHeaders(Context& ctx);


		std::vector<DirEntry> 	getDirectoryEntries(Context& ctx);
		void	generateAutoIndexBody(const std::vector<DirEntry>& entries, std::stringstream& content);
		void	generateAutoIndexHeader(Context& ctx, std::stringstream& content);
		bool	parseHeaderFields(Context& ctx, std::istringstream& stream);
		bool	parseRequestLine(Context& ctx, std::istringstream& stream);
		std::vector<std::string>	splitPathLoc(const std::string& path);
		std::vector<std::string>	getBlocksLocsPath(const std::vector<Location>& locations);
		bool	prepareUploadPingPong(Context& ctx);
		void	handleSessionCookies(Context& ctx);
		std::string	retreiveReqRoot(Context &ctx);
		bool isMultipart(Context& ctx);
		bool extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx);
		bool executeCgi(Context& ctx);
		std::vector<std::string> prepareCgiEnvironment(const Context& ctx);
		std::string extractQueryString(const std::string& path);
		bool parseContentDisposition(Context& ctx);
		bool fileExists(const std::string& path);
		bool isDirectory(const std::string& path);
		size_t getFileSize(const std::string& path);
		bool sendCgiResponse(Context& ctx);
		bool parseCGIBody(Context& ctx);
		bool isPathInUploadStore(Context& ctx, const std::string& path_to_check);
		bool handleCgiPipeEvent(int incoming_fd);
		void cleanupCgiResources(Context& ctx);
		bool checkAndReadCgiPipe(Context& ctx);
		bool prepareCgiHeaders(Context& ctx);
		bool buildDownloadResponse(Context &ctx);
		bool buildDownloadHeaders(Context &ctx);
		bool buildDownloadRead(Context &ctx);
		bool buildDownloadSend(Context &ctx);
		// server_init
		int initEpoll();
		bool initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs);

};

std::string		extractHostname(const std::string& header);
void			printServerBlock(ServerBlock& serverBlock);
void 			log_global_fds(const GlobalFDS& fds);
void 			log_server_configs(const std::vector<ServerBlock>& configs);
bool 			updateErrorStatus(Context &ctx, int error_code, std::string error_string);
int				extractPort(const std::string& header);
std::string		determineContentType(const std::string& path);
bool			isPathMatch(const std::vector<std::string>& requestSegments, const std::vector<std::string>& locationSegments);

#endif
