#ifndef STRUCTS_WEBSERV_HPP
# define STRUCTS_WEBSERV_HPP

#include "../main.hpp"


struct Location
{
	int			port;

	std::string	path;
	std::string	methods;
	std::string	autoindex;
	std::string	default_file;
	std::string	upload_store{""};
	long	client_max_body_size;
	std::string	root;
	std::string	cgi;
<<<<<<< HEAD
	std::string	cgi_filetype;
	std::string return_code;
	std::string return_url;
	bool doAutoindex{true};
=======
	std::string	cgi_param;
	std::string	redirect;
>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
	bool allowGet{true};
	bool allowPost{false};
	bool allowDelete{false};
	bool allowCookie{false};
};

struct ServerBlock
{
	int						port;
	int						server_fd;

	std::string				name;
	std::string				root;
	std::string				index;

	std::map				<int, std::string> errorPages;
	long					client_max_body_size;
	std::vector<Location>	locations;
};

struct RequestState
{
<<<<<<< HEAD
	int cur_fd;
=======
>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
	int client_fd;
	int cgi_in_fd;
	int cgi_out_fd;
	pid_t cgi_pid;
	bool cgi_done;
	bool is_directory{false};

	enum State {
		STATE_READING_REQUEST,
		STATE_PREPARE_CGI,
		STATE_CGI_RUNNING,
		STATE_SENDING_RESPONSE,
<<<<<<< HEAD
=======
		STATE_HTTP_PROCESS,
>>>>>>> 1ec4307b30a22d08ab0e6037b29cb5fe777feb10
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

	std::string method;  // e.g., "GET", "POST"
    std::string path;    // e.g., "/start_task"
	const ServerBlock* associated_conf;
	std::string location_path;
	std::string requested_path;

	bool keep_alive;
};


struct GlobalFDS
{
	int epoll_fd = -1;
	std::map<int, RequestState> request_state_map;
	std::map<int, int> svFD_to_clFD_map;
};

struct CgiTunnel {
	pid_t pid = -1;
	int in_fd = -1;
	int out_fd = -1;
	int client_fd = -1;
	int server_fd = -1;
	int	port;
	std::string server_name;
	const ServerBlock* config = NULL;
	const Location* location = NULL;
	std::chrono::steady_clock::time_point last_used;
	bool is_busy = false;
	std::string script_path;
	std::vector<char> buffer;
	CgiTunnel() : last_used(std::chrono::steady_clock::now()) {}
};

#endif
