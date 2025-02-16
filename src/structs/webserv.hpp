#ifndef STRUCTS_WEBSERV_HPP
# define STRUCTS_WEBSERV_HPP

#include "../server/server.hpp"
#include <deque>
#include <string>
#include <map>
#include <chrono>
#include <vector>

#define BUFFER_SIZE 17000
#define DEFAULT_REQUESTBUFFER_SIZE 16000
#define DEFAULT_MAXBODYSIZE 1048576

struct ChunkedState
{
	bool processing = false;
	std::string buffer;
};

// struct to save the location from each serverblock
struct Location
{
	int			port;
	std::string	path;
	std::string	methods;
	std::string	autoindex;
	std::string	default_file;
	std::string	upload_store{""};
	long long		client_max_body_size = -1;
	std::string	root;
	std::string	cgi;
	std::string	cgi_filetype;
	std::string	return_code;
	std::string	return_url;\
	bool		doAutoindex{true};
	bool		allowGet{true};
	bool		allowPost{false};
	bool		allowDelete{false};
};

// struct to save the nginx serverblocks
struct ServerBlock
{
	int port;
	int server_fd;

	std::string name;
	std::string root;
	std::string index;

	std::map<int, std::string>	errorPages;
	long long			client_max_body_size = -1;
	std::vector<Location>		locations;
	int							timeout = 30;
};

// status from the Request header
enum RequestType
{
	INITIAL,
	STATIC,
	CGI,
	REDIRECT,
	ERROR
};

enum ERRORFLAG {
	FILE_EXISTS  = F_OK,
	FILE_READ    = R_OK,
	FILE_WRITE   = W_OK,
	FILE_EXECUTE = X_OK
};

// Request body
struct RequestBody
{
	enum ParsingPhase
	{
		PARSING_HEADER,
		PARSING_BODY,
		PARSING_COMPLETE
	};

	long long 		content_length = 0;
	int			cgi_in_fd;
	int			cgi_out_fd;
	pid_t		cgi_pid;
	bool		cgi_done;
	bool		is_directory{false};
	bool		cgiMethodChecked{false};
	bool		clientMethodChecked{false};

	enum State {
		STATE_READING_REQUEST,
		STATE_PREPARE_CGI,
		STATE_CGI_RUNNING,
		STATE_SENDING_RESPONSE
	} state;

	enum Task
	{
		PENDING,
		IN_PROGRESS,
		COMPLETED
	} task;
	int pipe_fd{-1};

	std::string content_type;
	std::string request_body;
	std::string cookie_header;


	std::deque<char> response_buffer;
	std::deque<char> request_buffer;
	std::deque<char> cgi_output_buffer;

	std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();

;
	static constexpr std::chrono::seconds TIMEOUT_DURATION{5};

	ServerBlock* associated_conf;
	std::string location_path;
	std::string requested_path;

	ChunkedState chunked_state;

	int status_code;

	std::string	received_body;
	size_t		expected_body_length;
	size_t		current_body_length;
	bool		is_upload_complete;
	bool	keep_alive;

	ParsingPhase parsing_phase { PARSING_HEADER };
};

// Request header
struct Context
{
	int epoll_fd = -1;
	int client_fd = -1;
	int server_fd = -1;
	RequestType type = INITIAL;

	std::string method = "";
	std::string path = "";
	std::string version = "";
	std::map<std::string, std::string> headers;

	std::string body = "";
	std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::time_point();

	static constexpr std::chrono::seconds TIMEOUT_DURATION{5};

	std::string location_path = "";
	std::string requested_path = "";

	bool location_inited = false;
	Location location;
	RequestBody req;

	int error_code = 0;
	std::string error_message = "";
	int port;
	std::string name;
	std::string root;
	std::string index;
	std::string approved_req_path;

	std::map<int, std::string>	errorPages;
	long long						client_max_body_size = -1;
	int							timeout;
	std::string doAutoIndex = "";

	std::string read_buffer = "";
	std::string output_buffer = "";
	std::string tmp_buffer = "";
	bool headers_complete = false;
	size_t header_length = 0;
	long long content_length = 0;
	long long body_received = 0;
	bool keepAlive = false;
	bool is_multipart;
	bool had_seq_parse = false;
	std::vector<std::pair<std::string, std::string>> setCookies;
	std::vector<std::pair<std::string, std::string>> cookies;
	std::vector<std::string> blocks_location_paths;

	int upload_fd = -1;
	std::string uploaded_file_path = "";
	std::vector<char> write_buffer;
	size_t write_pos;
	size_t write_len;
	bool useLocRoot;
	size_t header_offset = 0;
	std::string boundary = "";
};

struct CgiTunnel
{
	pid_t pid = -1;
	int in_fd = -1;
	int out_fd = -1;
	int client_fd = -1;
	int server_fd = -1;
	int port = 0;
	std::string server_name;
	ServerBlock* config = NULL;
	Location* location = NULL;
	std::chrono::steady_clock::time_point last_used = std::chrono::steady_clock::now();
	bool is_busy = false;
	std::string script_path;
	std::vector<char> buffer;
	std::vector<char*> envp;
	RequestBody request;
};

struct GlobalFDS
{
	int epoll_fd = -1;
	std::map<int, Context> context_map;
	std::map<int, int> clFD_to_svFD_map;
};

struct DirEntry {
	std::string name;
	bool isDir;
	time_t mtime;
	off_t size;
};

struct Cookie {
	std::string name;
	std::string value;
	std::string domain;
	std::string path;
	time_t expires = 0;
	bool secure = false;
	bool httpOnly = false;
};

#endif