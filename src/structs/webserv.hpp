/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/14 17:31:47 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/29 11:27:07 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STRUCTS_WEBSERV_HPP
# define STRUCTS_WEBSERV_HPP

#include "../main.hpp"


struct Location
{
	int			port;

	std::string	path;
	std::string	methods;
	std::string	autoindex;
	std::string	return_directive;
	std::string	default_file;
	std::string	upload_store;
	std::string	client_max_body_size;
	std::string	root;
	std::string	cgi;
	std::string	cgi_param;
	std::string	redirect;
};

struct ServerBlock
{
	int						port;
	int						server_fd;

	std::string				name;
	std::string				root;
	std::string				index;

	std::map				<int, std::string> errorPages;
	std::string				client_max_body_size;
	std::vector<Location>	locations;
};

struct RequestState
{
    int client_fd;
    int cgi_in_fd;
    int cgi_out_fd;
    pid_t cgi_pid;
    bool cgi_done;

    enum State {
        STATE_READING_REQUEST,
        STATE_PREPARE_CGI,
        STATE_CGI_RUNNING,
        STATE_SENDING_RESPONSE
    } state;

    std::vector<char> request_buffer;
    std::vector<char> response_buffer;
    std::vector<char> cgi_output_buffer;

    std::chrono::steady_clock::time_point last_activity;

    static constexpr std::chrono::seconds TIMEOUT_DURATION{5}; // Correct initialization

    const ServerBlock* associated_conf;
    std::string requested_path;
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
	const ServerBlock* config = NULL;
	const Location* location = NULL;
	std::chrono::steady_clock::time_point last_used;
	bool is_busy = false;
	std::string script_path;
	std::vector<char> buffer;
	CgiTunnel() : last_used(std::chrono::steady_clock::now()) {}
};

#endif

//void StaticHandler::handleClientWrite(int epfd, int fd)
//{
//	std::stringstream ss;
//	ss << "Handling client write on fd " << fd;
//	Logger::file(ss.str());

//	RequestState &req = server.getGlobalFds().request_state_map[fd];
//	if (req.state == RequestState::STATE_SENDING_RESPONSE)
//	{
//		ss.str("");
//		ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
//		<< ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
//		Logger::file(ss.str());

//		int error = 0;
//		socklen_t len = sizeof(error);
//		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
//		{
//			Logger::file("Socket error detected: " + std::string(strerror(error)));
//			server.delFromEpoll(epfd, fd);
//			return;
//		}
//		ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

//		if (n > 0)
//		{
//			ss.str("");
//			ss << "Successfully wrote " << n << " bytes to client";
//			Logger::file(ss.str());

//			req.response_buffer.erase(
//				req.response_buffer.begin(),
//				req.response_buffer.begin() + n
//			);

//			if (req.response_buffer.empty())
//			{
//				Logger::file("Response fully sent, closing connection");
//				server.delFromEpoll(epfd, fd);
//			} else {
//				server.modEpoll(epfd, fd, EPOLLOUT);
//			}
//		}
//		else if (n < 0)
//		{
//			if (errno != EAGAIN && errno != EWOULDBLOCK)
//			{
//				ss.str("");
//				ss << "Write error: " << strerror(errno);
//				Logger::file(ss.str());
//				server.delFromEpoll(epfd, fd);
//			}
//			else
//			{
//				server.modEpoll(epfd, fd, EPOLLOUT);
//			}
//		}
//	}
//}
