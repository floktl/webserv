#include "Server.hpp"

void Server::modEpoll(int epoll_fd, int fd, uint32_t events) {
	if (fd <= 0) {
        Logger::file("WARNING: Attempt to modify epoll for invalid fd: " + std::to_string(fd));
        return;
    }
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		if (errno != EEXIST) {
			Logger::file("Epoll add failed: " + std::string(strerror(errno)));
			return;
		}
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
			Logger::file("Epoll mod failed: " + std::string(strerror(errno)));
		}
	}
}

void Server::setTimeout(int t) {
	timeout = t;
}

int Server::getTimeout() const {
	return timeout;
}


void Server::delFromEpoll(int epfd, int fd)
{
	 if (epfd <= 0 || fd <= 0) {
        Logger::file("WARNING: Attempt to delete invalid fd: " + std::to_string(fd));
        return;
    }
	if (findServerBlock(configData, fd)) {
		Logger::file("Prevented removal of server socket: " + std::to_string(fd));
		return;
	}
	//Logger::file("Removing fd " + std::to_string(fd) + " from epoll");

	if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
		Logger::file("Error removing from epoll: " + std::string(strerror(errno)));

	auto ctx_it = globalFDS.request_state_map.find(fd);
	if (ctx_it != globalFDS.request_state_map.end()) {
		RequestBody &req = ctx_it->second.req;
		if (req.cgi_in_fd != -1) {
			close(req.cgi_in_fd);
			globalFDS.clFD_to_svFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1) {
			close(req.cgi_out_fd);
			globalFDS.clFD_to_svFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}
	Logger::file("removed from clFD_to_svFD_map[clFD:" + std::to_string(fd) + "] = svFD(" + std::to_string(globalFDS.clFD_to_svFD_map[fd]) + ")");
	close(fd);
	globalFDS.clFD_to_svFD_map.erase(fd);

	//Logger::file("Successfully removed fd " + std::to_string(fd));
}

bool Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	//Logger::file("Finding ServerBlock for fd: " + std::to_string(fd));
	for (const auto &conf : configs) {
		////Logger::file("Checking config with fd: " + std::to_string(conf.server_fd));
		if (conf.server_fd == fd) {
			//Logger::file("Found matching ServerBlock with fd: " + std::to_string(conf.server_fd) + " on Port: " + std::to_string(conf.port));
			return true;
		}
	}
	//Logger::file("No matching ServerBlock found");
	return false;
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// void Server::setTaskStatus(enum RequestBody::Task new_task, int client_fd)
// {
// 	if (client_fd < 0)
// 	{
// 		//Logger::file("Error: Invalid client_fd " + std::to_string(client_fd));
// 		return;
// 	}

// 	auto it = globalFDS.request_state_map.find(client_fd);

// 	if (it != globalFDS.request_state_map.end())
// 	{
// 		it->second.task = new_task;

// 		std::string taskName = (new_task == RequestBody::PENDING ? "PENDING" :
// 							new_task == RequestBody::IN_PROGRESS ? "IN_PROGRESS" :
// 							new_task == RequestBody::COMPLETED ? "COMPLETED" :
// 							"UNKNOWN");

// 	}
// 	else
// 	{
// 		//Logger::file("Error: client_fd " + std::to_string(client_fd) + " not found in request_state_map");
// 	}
// }


// enum RequestBody::Task Server::getTaskStatus(int client_fd)
// {
// 	auto it = globalFDS.request_state_map.find(client_fd);

// 	if (it != globalFDS.request_state_map.end())
// 	{
// 		return it->second.task;
// 	}
// 	else
// 	{
// 		//Logger::file("Error: client_fd " + std::to_string(client_fd) + " not found in request_state_map");
// 		return RequestBody::Task::PENDING;
// 	}
// }

bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	// Check if the server_name already exists in /etc/hosts
	while (std::getline(infile, line)) {
		if (line.find(server_name) != std::string::npos) {
			return true; // Already present, no need to add
		}
	}

	// Add server_name to /etc/hosts
	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();
	Logger::yellow("Added " + server_name + " to /etc/hosts file");
	// Store the added server name
	added_server_names.push_back(server_name);
	return true;
}

void Server::removeAddedServerNamesFromHosts()
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	if (!infile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(infile, line)) {
		bool shouldRemove = false;
		for (const auto &name : added_server_names) {
			if (line.find(name) != std::string::npos) {
				Logger::yellow("Remove " + name + " from /etc/host file");
				shouldRemove = true;
				break;
			}
		}
		if (!shouldRemove) {
			lines.push_back(line);
		}
	}
	infile.close();

	std::ofstream outfile(hosts_file, std::ios::trunc);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts for writing");
	}
	for (const auto &l : lines) {
		outfile << l << "\n";
	}
	outfile.close();

	// Clear the added server names
	added_server_names.clear();
}
