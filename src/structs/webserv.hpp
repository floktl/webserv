#ifndef STRUCTS_WEBSERV_HPP
# define STRUCTS_WEBSERV_HPP

#include "../main.hpp"
#include <deque>
#include <string>
#include <map>
#include <chrono>
#include <vector>

struct ChunkedState {
	bool processing;
	std::string buffer;

	ChunkedState() : processing(false) {}
};

struct Location
{
	int			port;
	std::string	path;
	std::string	methods;
	std::string	autoindex;
	std::string	default_file;
	std::string	upload_store{""};
	long		client_max_body_size;
	std::string	root;
	std::string	cgi;
	std::string	cgi_filetype;
	std::string	return_code;
	std::string	return_url;
	bool		doAutoindex{true};
	bool		allowGet{true};
	bool		allowPost{false};
	bool		allowDelete{false};
	bool		allowCookie{false};
};

struct ServerBlock
{
	int port;
	int server_fd;

	std::string name;
	std::string root;
	std::string index;

	std::map<int, std::string>	errorPages;
	long						client_max_body_size;
	std::vector<Location>		locations;
	int							timeout;

	ServerBlock() : timeout(30) {}
};

struct RequestState
{
	enum ParsingPhase {
		PARSING_HEADER,
		PARSING_BODY,
		PARSING_COMPLETE
	};

	std::string	method;
	size_t		content_length = 0;
	int			client_fd;
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

	enum Task {
		PENDING,
		IN_PROGRESS,
		COMPLETED
	} task;
	int pipe_fd{-1};

	std::string content_type;
	std::string request_body;
	std::string cookie_header;

	std::deque<char> request_buffer;
	std::deque<char> response_buffer;
	std::deque<char> cgi_output_buffer;

	std::chrono::steady_clock::time_point last_activity;
	static constexpr std::chrono::seconds TIMEOUT_DURATION{5};

	const ServerBlock* associated_conf;
	std::string location_path;
	std::string requested_path;

	ChunkedState chunked_state;

	int status_code;

	std::string	received_body;
	size_t		expected_body_length;
	size_t		current_body_length;
	bool		is_upload_complete;
	std::string	uploaded_file_path;
	bool	keep_alive;
	bool is_multipart;

	ParsingPhase parsing_phase { PARSING_HEADER };
};


struct CgiTunnel {
	pid_t pid = -1;
	int in_fd = -1;
	int out_fd = -1;
	int client_fd = -1;
	int server_fd = -1;
	int port = 0;
	std::string server_name;
	const ServerBlock* config = NULL;
	const Location* location = NULL;
	std::chrono::steady_clock::time_point last_used = std::chrono::steady_clock::now();
	bool is_busy = false;
	std::string script_path;
	std::vector<char> buffer;
	std::vector<char*> envp;
	RequestState request;
};


enum RequestType {
	INITIAL,
	STATIC,
	CGI,
	ERROR
};

struct Context
{
	int epoll_fd = -1;
	int client_fd = -1;
	int server_fd = -1;
	RequestType type;
	std::string method;
	std::string path;
	std::string version;
	std::map<std::string, std::string> headers;
	std::string body;
	std::chrono::steady_clock::time_point last_activity;
	static constexpr std::chrono::seconds TIMEOUT_DURATION{5};

	std::string location_path;
	std::string requested_path;
	const Location* location;
};

struct GlobalFDS
{
	int epoll_fd = -1;
	std::map<int, Context> request_state_map;
	std::map<int, int> svFD_to_clFD_map;
};

#endif