#ifndef STRUCTS_WEBSERV_HPP
# define STRUCTS_WEBSERV_HPP

#include "../server/server.hpp"
#include <deque>
#include <string>
#include <map>
#include <chrono>
#include <vector>

#define DEFAULT_FILE				"index.html"
#define DEFAULT_REQUESTBUFFER_SIZE	8192
#define DEFAULT_CGIBUFFER_SIZE		65536
#define DEFAULT_MAXBODYSIZE			1048576
#define MAX_EVENTS					64
#define TIMEOUT_MS					1000
#define DEFAULT_TIMEOUT				10

// Struct to save the location from each server block
struct Location
{
	int				port;
	std::string		path;
	std::string		methods;
	std::string		autoindex;
	std::string		default_file;
	std::string		upload_store{"uploads"};
	long long		client_max_body_size = -1;
	std::string		root;
	std::string		cgi;
	std::string		cgi_filetype;
	std::string		return_code;
	std::string		return_url;
	bool			allowGet{true};
	bool			allowPost{false};
	bool			allowDelete{false};
};

// Struct to save the Nginx server blocks
struct ServerBlock
{
	int 						port;
	int 						server_fd;

	std::string 				name;
	std::string 				root;
	std::string 				index;

	std::map<int, std::string>	errorPages;
	long long					client_max_body_size = -1;
	std::vector<Location>		locations;
	int							timeout = 30;
};

// Status from the Request header
enum RequestType
{
	INITIAL,
	STATIC,
	CGI,
	REDIRECT,
	ERROR
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

	long long 	content_length = 0;
	int			cgi_in_fd = -1;
	int			cgi_out_fd = -1;
	pid_t		cgi_pid = -1;


	std::deque<char> response_buffer;
	std::deque<char> request_buffer;
	std::deque<char> cgi_output_buffer;

	size_t			expected_body_length = 0;
	size_t			current_body_length = 0;
	bool			is_upload_complete = false;

	ParsingPhase	parsing_phase { PARSING_HEADER };
};

// Request header
struct Context
{
	int			epoll_fd = -1;
	int			client_fd = -1;
	int			server_fd = -1;
	RequestType type = INITIAL;

	std::string							method = "";
	std::string							path = "";
	std::string							version = "";
	std::map<std::string, std::string>	headers;
	std::string							body = "";

	std::chrono::steady_clock::time_point	last_activity = std::chrono::steady_clock::time_point();

	std::string	location_path = "";
	std::string	requested_path = "";
	bool		location_inited = false;
	Location	location;

	RequestBody req;

	int							error_code = 0;
	std::string					error_message = "";
	std::map<int, std::string>	errorPages;

	int			port;
	std::string	name;
	std::string	root;
	std::string	index;
	std::string	approved_req_path;

	long long	client_max_body_size = -1;
	int			timeout = DEFAULT_TIMEOUT;
	std::string do_autoindex = "";

	std::string read_buffer = "";
	std::vector<std::pair<std::string, std::string>> set_cookies;
	std::vector<std::pair<std::string, std::string>> cookies;
	std::vector<std::string> blocks_location_paths;

	int					multipart_fd_up_down = -1;
	std::string			multipart_file_path_up_down = "";
	std::vector<char>	write_buffer;
	bool				use_loc_root;
	size_t				header_offset = 0;
	std::string			boundary = "";
	bool				found_first_boundary = false;
	bool				is_download = false;
	bool				final_boundary_found = false;
	bool				ready_for_ping_pong = false;
	bool				headers_complete = false;
	bool				keepAlive = false;
	bool				is_multipart = false;
	bool				download_headers_sent = false;
	bool				download_phase = false;
	bool				cgi_output_phase = false;
	bool				cgi_headers_send = false;
	bool				cgi_executed = false;
	bool				cgi_terminate = false;
	bool				cgi_terminated = false;
	bool				was_cgi_del = false;
	bool				cgi_pipe_ready = false;
	bool				cgi_run_to_timeout = false;
	std::chrono::steady_clock::time_point cgi_start_time = std::chrono::steady_clock::now();

};

struct GlobalFDS
{
	int						epoll_fd = -1;
	std::map<int, Context>	context_map;
	std::map<int, int>		clFD_to_svFD_map;
	std::map<int, int>		cgi_pipe_to_client_fd;
	std::map<pid_t, int>	cgi_pid_to_client_fd;
};

struct DirEntry
{
	std::string	name;
	bool		isDir;
	time_t		mtime;
	off_t		size;
};

struct Cookie
{
	std::string	name = "";
	std::string	value = "";
	std::string	path = "";
	time_t		expires = 0;
};

#endif