/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jonathaneberle <jonathaneberle@student.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:26 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/19 16:41:53 by jonathanebe      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "StaticHandler.hpp"

StaticHandler::StaticHandler(Server& _server) : server(_server) {}

void StaticHandler::handleClientRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client read on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = server.getGlobalFds().request_state_map[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0) {
		Logger::file("Client closed connection");
		server.delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			Logger::file("Read error: " + std::string(strerror(errno)));
			server.delFromEpoll(epfd, fd);
		}
		return;
	}

	ss.str("");
	ss << "Read " << n << " bytes from client";
	Logger::file(ss.str());

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4) {
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {
			Logger::file("Complete request received, parsing");
			server.getRequestHandler()->parseRequest(req);

			if (!server.getCgiHandler()->needsCGI(req.associated_conf, req.requested_path)) {
				Logger::file("Processing as normal request");
				std::string filepath = req.associated_conf->root;
				if (filepath.back() != '/') filepath += '/';
				filepath += req.requested_path;

				FileState state;
				state.file.open(filepath, std::ios::binary);
				if (!state.file.is_open()) {
					server.getRequestHandler()->buildResponse(req);
					req.state = RequestState::STATE_SENDING_RESPONSE;
					server.modEpoll(epfd, fd, EPOLLOUT);
					return;
				}

				state.file.seekg(0, std::ios::end);
				state.remaining_bytes = state.file.tellg();
				state.file.seekg(0, std::ios::beg);
				state.headers_sent = false;

				file_states[fd] = std::move(state);
				req.state = RequestState::STATE_SENDING_RESPONSE;
				server.modEpoll(epfd, fd, EPOLLOUT);
			} else {
				Logger::file("Request requires CGI processing");
			}
		}
	}
}

void StaticHandler::handleClientWrite(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client write on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = server.getGlobalFds().request_state_map[fd];
	if (req.state == RequestState::STATE_SENDING_RESPONSE) {
		auto file_it = file_states.find(fd);
		if (file_it != file_states.end()) {
			FileState &state = file_it->second;

			if (!state.headers_sent) {
				std::stringstream headers;
				headers << "HTTP/1.1 200 OK\r\n"
					<< "Content-Length: " << state.remaining_bytes << "\r\n"
					<< "Connection: close\r\n\r\n";
				std::string header_str = headers.str();
				ssize_t sent = send(fd, header_str.c_str(), header_str.length(), MSG_NOSIGNAL);
				if (sent < 0) {
					if (errno != EAGAIN && errno != EWOULDBLOCK) {
						file_states.erase(fd);
						server.delFromEpoll(epfd, fd);
					}
					return;
				}
				state.headers_sent = true;
			}

			char buffer[CHUNK_SIZE];
			size_t to_read = (state.remaining_bytes < CHUNK_SIZE) ?
				state.remaining_bytes : static_cast<size_t>(CHUNK_SIZE);
			state.file.read(buffer, to_read);
			size_t actual_read = state.file.gcount();

			if (actual_read > 0) {
				ssize_t sent = send(fd, buffer, actual_read, MSG_NOSIGNAL);
				if (sent > 0) {
					state.remaining_bytes -= sent;
					if (sent < static_cast<ssize_t>(actual_read)) {
						state.file.seekg(sent - actual_read, std::ios::cur);
					}
					if (state.remaining_bytes > 0) {
						server.modEpoll(epfd, fd, EPOLLOUT);
						return;
					}
				} else if (sent < 0) {
					if (errno != EAGAIN && errno != EWOULDBLOCK) {
						file_states.erase(fd);
						server.delFromEpoll(epfd, fd);
						return;
					}
					server.modEpoll(epfd, fd, EPOLLOUT);
					return;
				}
			}
			state.file.close();
			file_states.erase(fd);
			server.delFromEpoll(epfd, fd);
			return;
		}

		ss.str("");
		ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
			<< ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
		Logger::file(ss.str());

		int error = 0;
		socklen_t len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
			Logger::file("Socket error detected: " + std::string(strerror(error)));
			server.delFromEpoll(epfd, fd);
			return;
		}

		ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

		if (n > 0) {
			ss.str("");
			ss << "Successfully wrote " << n << " bytes to client";
			Logger::file(ss.str());

			req.response_buffer.erase(
				req.response_buffer.begin(),
				req.response_buffer.begin() + n
			);

			if (req.response_buffer.empty()) {
				Logger::file("Response fully sent, closing connection");
				server.delFromEpoll(epfd, fd);
			} else {
				server.modEpoll(epfd, fd, EPOLLOUT);
			}
		} else if (n < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				ss.str("");
				ss << "Write error: " << strerror(errno);
				Logger::file(ss.str());
				server.delFromEpoll(epfd, fd);
			} else {
				server.modEpoll(epfd, fd, EPOLLOUT);
			}
		}
	}
}
