/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   webserv.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/14 17:31:47 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/17 20:01:22 by fkeitel          ###   ########.fr       */
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
	std::chrono::steady_clock::time_point last_used;
	bool is_busy = false;
	std::string script_path;
	std::vector<char> buffer;
	CgiTunnel() : last_used(std::chrono::steady_clock::now()) {}
};

#endif
