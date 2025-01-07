#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../main.hpp"
#include <vector>
#include <string>
#include <sstream>
#include <dirent.h>
#include <algorithm>
#include <sys/stat.h>
#include <ctime>

struct RequestState;

class CgiHandler;
class Server;
class TaskManager;

class RequestHandler
{
public:
	struct DirEntry {
		std::string name;
		bool isDir;
		time_t mtime;
		off_t size;
	};

	RequestHandler(Server& server);

	// Parse functions
	void			parseRequest(RequestState &req);
	// Utility functions
	const Location*	findMatchingLocation(const ServerBlock* conf, const std::string& path);
	std::string getMethod(const std::deque<char>& request_buffer);

	void buildErrorResponse(int statusCode, const std::string& message, std::stringstream *response, RequestState &req);
private:
	Server& server;

	// Build response functions
	void buildResponse(RequestState &req);
	void handlePostRequest(RequestState &req);
	void handleDeleteRequest(RequestState &req);
	void handleOtherRequests(RequestState &req, const Location* loc);
	void buildAutoindexResponse(std::stringstream* response, RequestState& req);


	// Utility functions
	bool		checkRedirect(RequestState &req, std::stringstream *response);
	std::string buildRequestedPath(RequestState &req, const std::string &rawPath);

	// Task functions
	void handleTaskRequest(std::stringstream* response);
	void handleStatusRequest(const std::string& taskId, std::stringstream* response);

	// Autoindex helpers
	bool checkDirectoryPermissions(const RequestState& req, std::stringstream* response);
	std::vector<DirEntry> getDirectoryEntries(RequestState& req, std::stringstream* response);
	void processDirectoryEntry(const RequestState& req, struct dirent* dir_entry, std::vector<DirEntry>& entries);
	void sortDirectoryEntries(std::vector<DirEntry>& entries);
	void generateAutoindexHtml(std::stringstream* response, const RequestState& req, const std::vector<DirEntry>& entries);
	void generateEntryHtml(std::stringstream* response, const DirEntry& entry);
	std::string formatFileSize(off_t size);

	// Helper functions for parsing requests
	void  make_respone(RequestState &req, std::string method, std::string path, std::string query);
	size_t findHeaderEnd(const std::string &request);
	std::string extractHeaders(const std::string &request, size_t header_end);
	void parseRequestLine(const std::string &headers, std::string &method, std::string &path, std::string &version);
	void processQueryString(std::string &path, std::string &query);
	void processHeaders(const std::string &headers, RequestState &req);
	std::string extractRequestBody(const std::string &request, size_t header_end);
	bool handleRedirect(RequestState &req);

	// Helper functions for requested path
	std::string extractUsedRoot(const Location* loc, const ServerBlock* conf);
	std::string processPathAfterLocation(const std::string& rawPath, const Location* loc);
	std::string handleDefaultFile(const std::string& fullPath, const Location* loc, RequestState& req);
	std::string handleDirectoryRequest(const std::string& fullPath, const Location* loc,
		const ServerBlock* conf, RequestState& req);
};

#endif // REQUESTHANDLER_HPP
