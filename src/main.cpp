/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 10:09:54 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "main.hpp"

int g_epfd = -1; // Initial value
std::map<int, RequestState> g_requests;
std::map<int, int> g_fd_to_client;

static void handleNewConnection(int epfd, int fd, const ServerBlock& conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
		Logger::file("Accept failed: " + std::string(strerror(errno)));
		return;
	}

	setNonBlocking(client_fd);
	modEpoll(epfd, client_fd, EPOLLIN);

	RequestState req;
	req.client_fd = client_fd;
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
	req.cgi_pid = -1;
	req.state = RequestState::STATE_READING_REQUEST;
	req.cgi_done = false;
	req.associated_conf = &conf;
	g_requests[client_fd] = req;

	std::stringstream ss;
	ss << "New client connection on fd " << client_fd;
	Logger::file(ss.str());
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	g_epfd = -1;
	try
	{
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid())
		{
			Logger::red() << "Invalid configuration file!";
			Logger::file("Invalid configuration file");
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			Logger::file("No configurations found");
			return EXIT_FAILURE;
		}

		int epfd = epoll_create1(0);
		if (epfd < 0)
		{
			Logger::red() << "Failed to create epoll\n";
			Logger::file("Failed to create epoll: " + std::string(strerror(errno)));
			return EXIT_FAILURE;
		}
		g_epfd = epfd;
		Logger::file("Server starting");

		for (auto &conf : configs)
		{
			conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
			if (conf.server_fd < 0)
			{
				std::stringstream ss;
				ss << "Failed to create socket on port: " << conf.port;
				Logger::red() << ss.str();
				Logger::file(ss.str());
				return EXIT_FAILURE;
			}

			int opt = 1;
			setsockopt(conf.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

			struct sockaddr_in addr;
			memset(&addr, 0, sizeof(addr));
			addr.sin_family = AF_INET;
			addr.sin_port = htons(conf.port);
			addr.sin_addr.s_addr = htonl(INADDR_ANY);

			if (bind(conf.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
			{
				std::stringstream ss;
				ss << "Failed to bind socket on port: " << conf.port;
				Logger::red() << ss.str();
				Logger::file(ss.str());
				close(conf.server_fd);
				return EXIT_FAILURE;
			}

			if (listen(conf.server_fd, SOMAXCONN) < 0)
			{
				std::stringstream ss;
				ss << "Failed to listen on port: " << conf.port;
				Logger::red() << ss.str();
				Logger::file(ss.str());
				close(conf.server_fd);
				return EXIT_FAILURE;
			}

			setNonBlocking(conf.server_fd);
			modEpoll(epfd, conf.server_fd, EPOLLIN);

			std::stringstream ss;
			ss << "Server listening on port: " << conf.port;
			Logger::green() << ss.str() << "\n";
			Logger::file(ss.str());
		}

		const int max_events = 64;
		struct epoll_event events[max_events];

		while (true)
		{
			int n = epoll_wait(epfd, events, max_events, -1);

			std::stringstream wer;
			wer << "epoll_wait n " << n;
			Logger::file(wer.str());
			if (n < 0)
			{
				std::stringstream ss;
				ss << "epoll_wait error: " << strerror(errno);
				Logger::file(ss.str());
				if (errno == EINTR)
					continue;
				break;
			}

			std::stringstream ewrew;
			ewrew << "Event Loop Iteration: " << n << " events";
			Logger::file(ewrew.str());

			for (int i = 0; i < n; i++)
			{
				int fd = events[i].data.fd;
				uint32_t ev = events[i].events;

				std::stringstream ss;
				ss << "\n=== Event Processing Start ===\n"
				<< "Processing fd=" << fd << " events=" << ev << "\n"
				<< "EPOLLIN=" << (ev & EPOLLIN) << "\n"
				<< "EPOLLOUT=" << (ev & EPOLLOUT) << "\n"
				<< "EPOLLHUP=" << (ev & EPOLLHUP) << "\n"
				<< "EPOLLERR=" << (ev & EPOLLERR);
				Logger::file(ss.str());

				const ServerBlock* associated_conf = nullptr;
				for (const auto &conf : configs)
				{
					if (conf.server_fd == fd)
					{
						associated_conf = &conf;
						break;
					}
				}

				if (associated_conf)
				{
					handleNewConnection(epfd, fd, *associated_conf);
					continue;
				}

				auto client_it = g_requests.find(fd);
				if (client_it != g_requests.end())
				{
					if (ev & (EPOLLHUP | EPOLLERR))
					{
						Logger::file("Client socket error/hangup detected");
						delFromEpoll(epfd, fd);
						continue;
					}
					if (ev & EPOLLIN) handleClientRead(epfd, fd);
					if (ev & EPOLLOUT) handleClientWrite(epfd, fd);
					continue;
				}

				auto cgi_it = g_fd_to_client.find(fd);
				if (cgi_it != g_fd_to_client.end())
				{
					int client_fd = cgi_it->second;
					RequestState &req = g_requests[client_fd];

					ss.str("");
					ss << "CGI pipe event:\n"
					<< "- Related client_fd: " << client_fd << "\n"
					<< "- CGI in_fd: " << req.cgi_in_fd << "\n"
					<< "- CGI out_fd: " << req.cgi_out_fd << "\n"
					<< "- Current state: " << req.state;
					Logger::file(ss.str());

					if (fd == req.cgi_out_fd)
					{
						if (ev & EPOLLIN)
						{
							handleCGIRead(epfd, fd);
						}
						if (ev & EPOLLHUP)
						{
							Logger::file("CGI output pipe hangup detected, finalizing response");

							if (!req.cgi_output_buffer.empty())
							{
								std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
								ss.str("");
								ss << "Preparing final response with " << output.length() << " bytes";
								Logger::file(ss.str());

								std::string response;
								size_t header_end = output.find("\r\n\r\n");

								if (output.find("Content-type:") != std::string::npos ||
									output.find("Content-Type:") != std::string::npos)
								{
									std::string headers = output.substr(0, header_end);
									std::string body = output.substr(header_end + 4);

									response = "HTTP/1.1 200 OK\r\n";
									if (headers.find("Content-Length:") == std::string::npos)
									{
										response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
									}
									if (headers.find("Connection:") == std::string::npos)
									{
										response += "Connection: close\r\n";
									}
									response += headers + "\r\n" + body;
								}
								else
								{
									response = "HTTP/1.1 200 OK\r\n"
											"Content-Type: text/html\r\n"
											"Content-Length: " + std::to_string(output.length()) + "\r\n"
											"Connection: close\r\n"
											"\r\n" + output;
								}

								ss.str("");
								ss << "Final response headers:\n" << response.substr(0, response.find("\r\n\r\n") + 4);
								Logger::file(ss.str());

								req.response_buffer.assign(response.begin(), response.end());
								req.state = RequestState::STATE_SENDING_RESPONSE;
								modEpoll(epfd, client_fd, EPOLLOUT);
							}
							cleanupCGI(req);
						}
					}
					else if (fd == req.cgi_in_fd)
					{
						if (ev & EPOLLOUT)
						{
							handleCGIWrite(epfd, fd);
						}
						if (ev & (EPOLLHUP | EPOLLERR))
						{
							Logger::file("CGI input pipe error/hangup detected");
							epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
							close(fd);
							req.cgi_in_fd = -1;
							g_fd_to_client.erase(fd);
						}
					}
					continue;
				}

				Logger::file("Unknown fd encountered, removing from epoll");
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				Logger::file("=== Event Processing End ===\n");
			}
		}
		Logger::file("Server shutting down");
		close(epfd);
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		Logger::file(err_msg);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
