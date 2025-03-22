#include "Server.hpp"

bool Server::buildDownloadResponse(Context &ctx)
{
	struct stat file_stat;
	if (stat(ctx.multipart_file_path_up_down.c_str(), &file_stat) != 0) {
		return updateErrorStatus(ctx, 404, "File not found");
	}

	std::string filename = ctx.multipart_file_path_up_down;
	size_t last_slash = filename.find_last_of("/\\");
	if (last_slash != std::string::npos) {
		filename = filename.substr(last_slash + 1);
	}

	std::string contentType = determineContentType(ctx.multipart_file_path_up_down);

	// First time - send headers
	if (!ctx.download_headers_sent) {
		std::stringstream response;
		response << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: " << contentType << "\r\n"
				<< "Content-Length: " << file_stat.st_size << "\r\n"
				<< "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

		for (const auto& cookiePair : ctx.setCookies) {
			Cookie cookie;
			cookie.name = cookiePair.first;
			cookie.value = cookiePair.second;
			cookie.path = "/";
			response << generateSetCookieHeader(cookie) << "\r\n";
		}

		response << "\r\n";

		if (send(ctx.client_fd, response.str().c_str(), response.str().size(), MSG_NOSIGNAL) < 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Failed to send headers");
		}

		ctx.download_headers_sent = true;
		ctx.download_phase = true; // Set to read phase first
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	}

	// Alternate between reading and sending
	if (ctx.download_phase) {
		char buffer[DEFAULT_REQUESTBUFFER_SIZE];
		ssize_t bytes_read = read(ctx.multipart_fd_up_down, buffer, sizeof(buffer));

		if (bytes_read < 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Failed to read file");
		}

		if (bytes_read == 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;

			if (ctx.keepAlive) {
				resetContext(ctx);
				return  modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
			} else {
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			return true;
		}

		ctx.write_buffer.assign(buffer, buffer + bytes_read);
		ctx.download_phase = false;
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	} else {
		if (ctx.write_buffer.size() > 0) {
			if (send(ctx.client_fd, ctx.write_buffer.data(), ctx.write_buffer.size(), MSG_NOSIGNAL) < 0) {
				close(ctx.multipart_fd_up_down);
				ctx.multipart_fd_up_down = -1;
				return updateErrorStatus(ctx, 500, "Failed to send file content");
			}
			ctx.req.current_body_length += ctx.write_buffer.size();
			Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "(" + std::to_string(ctx.multipart_fd_up_down) + ") Download 8");
			ctx.write_buffer.clear();
			ctx.download_phase = true; // Switch back to read phase
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		}
	}

	return false;
}
