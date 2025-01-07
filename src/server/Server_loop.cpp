#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs) {
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 5000;

	while (true) {
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);

		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		else if (n == 0) {
			checkAndCleanupTimeouts();
			taskManager->processTask(epoll_fd);
			continue;
		}

		for (int i = 0; i < n; i++) {
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			// Verwende getPipeReadFd() statt direktem Zugriff
			if (fd == taskManager->getPipeReadFd() && (ev & EPOLLIN)) {
				taskManager->processTask(epoll_fd);
				continue;
			}
			dispatchEvent(epoll_fd, fd, ev, configs);
		}

		checkAndCleanupTimeouts();
	}

	close(epoll_fd);
	return EXIT_SUCCESS;
}

bool Server::dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	if (const ServerBlock* conf = findServerBlock(configs, fd))
	{
		handleNewConnection(epoll_fd, fd, *conf);
		return true;
	}

	if (globalFDS.svFD_to_clFD_map.find(fd) != globalFDS.svFD_to_clFD_map.end())
	{
		handleCGIEvent(epoll_fd, fd, ev);
		return true;
	}

	if (globalFDS.request_state_map.find(fd) != globalFDS.request_state_map.end())
	{

		handleClientEvent(epoll_fd, fd, ev);
		return true;
	}

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	return false;
}

void Server::handleNewConnection(int epoll_fd, int fd, const ServerBlock &conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
		return;
	}

	setNonBlocking(client_fd);
	modEpoll(epoll_fd, client_fd, EPOLLIN);

	RequestState req;
	req.cur_fd = fd;
	req.client_fd = client_fd;
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
	req.cgi_pid = -1;
	req.state = RequestState::STATE_READING_REQUEST;
	req.cgi_done = false;
	req.associated_conf = &conf;
	globalFDS.request_state_map[client_fd] = req;
}

void Server::checkAndCleanupTimeouts() {
	auto now = std::chrono::steady_clock::now();

	for (auto it = globalFDS.request_state_map.begin(); it != globalFDS.request_state_map.end();) {
		RequestState &req = it->second;

		if (req.state == RequestState::STATE_CGI_RUNNING) {
			auto duration = std::chrono::duration_cast<std::chrono::seconds>(
				now - req.last_activity
			).count();

			if (duration > timeout) {

				// Kill the CGI process first
				killTimeoutedCGI(req);

				// Generate error response using ErrorHandler
				std::string error_response = getErrorHandler()->generateErrorResponse(
					504,  // Gateway Timeout
					"Gateway Timeout",
					req
				);

				// Insert the response into the buffer
				req.response_buffer.clear();
				req.response_buffer.insert(req.response_buffer.begin(),
										error_response.begin(),
										error_response.end());

				// Update state and epoll
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(globalFDS.epoll_fd, req.client_fd, EPOLLOUT);

				// Mark task as completed
				getTaskManager()->sendTaskStatusUpdate(req.client_fd, RequestState::COMPLETED);
			}
		}
		++it;
	}
}

void Server::killTimeoutedCGI(RequestState &req) {
	if (req.cgi_pid > 0) {
		kill(req.cgi_pid, SIGTERM);
		std::this_thread::sleep_for(std::chrono::microseconds(100000));
		int status;
		pid_t result = waitpid(req.cgi_pid, &status, WNOHANG);
		if (result == 0) {
			kill(req.cgi_pid, SIGKILL);
			waitpid(req.cgi_pid, &status, 0);
		}
		req.cgi_pid = -1;
	}
	if (req.cgi_in_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}
	if (req.cgi_out_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}
	if (req.pipe_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, NULL);
		close(req.pipe_fd);
		req.pipe_fd = -1;
	}
}