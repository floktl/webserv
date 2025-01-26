// #include "TaskManager.hpp"
// #include <sstream>
// #include <chrono>
// #include <thread>
// #include <iostream> // For debug statements

// TaskManager::TaskManager(Server& _server) : server(_server) {
// 	if (pipe(pipe_fd) < 0) {
// 		perror("pipe");
// 		throw std::runtime_error("Failed to create pipe for TaskManager");
// 	}
// 	int flags = fcntl(pipe_fd[0], F_GETFL, 0);
// 	fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);
// }

// TaskManager::~TaskManager() {
// 	close(pipe_fd[0]);
// 	close(pipe_fd[1]);
// }

// int TaskManager::getPipeReadFd() const {
// 	return pipe_fd[0];
// }

// std::string TaskManager::createTask() {
// 	std::lock_guard<std::mutex> lock(taskMutex);
// 	std::string taskId = "task_" + std::to_string(tasks.size() + 1);
// 	tasks[taskId] = Task{taskId, RequestState::PENDING, 0};  // Verwende RequestState::PENDING
// 	return taskId;
// }

// TaskManager::Task TaskManager::getTaskStatus(const std::string& taskId) {
// 	std::lock_guard<std::mutex> lock(taskMutex);
// 	if (tasks.find(taskId) != tasks.end()) {
// 		return tasks[taskId];
// 	}
// 	return Task{"", RequestState::COMPLETED, 100};  // Verwende RequestState::COMPLETED
// }

// void TaskManager::sendTaskStatusUpdate(int client_fd, RequestState::Task status) {
// 	TaskUpdate update;
// 	update.client_fd = client_fd;
// 	update.status = status;

// 	ssize_t bytes_written = write(pipe_fd[1], &update, sizeof(update));
// 	if (bytes_written != sizeof(update)) {
// 		perror("write to pipe");
// 	}
// }

// void TaskManager::handleTaskUpdates() {
// 	TaskUpdate update;
// 	ssize_t bytes_read;
// 	while ((bytes_read = read(pipe_fd[0], &update, sizeof(update))) > 0) {
// 		std::lock_guard<std::mutex> lock(taskMutex);
// 		auto& request_map = server.getGlobalFds().request_state_map;
// 		auto it = request_map.find(update.client_fd);
// 		if (it != request_map.end()) {
// 			it->second.task = update.status;  // Jetzt kompatibel
// 			std::cout << "[DEBUG] Task status updated for client_fd "
// 					<< update.client_fd << " to "
// 					<< (update.status == RequestState::PENDING ? "PENDING" :
// 						update.status == RequestState::IN_PROGRESS ? "IN_PROGRESS" : "COMPLETED")
// 					<< std::endl;
// 		}
// 	}

// 	if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
// 		perror("read from pipe");
// 	}
// }

// void TaskManager::processTask(int epoll_fd) {
// 	for (auto it = server.getGlobalFds().request_state_map.begin();
// 		it != server.getGlobalFds().request_state_map.end(); ) {
// 		int key = it->first;
// 		RequestState& req = it->second;

// 		if (req.pipe_fd != -1) {
// 			RequestState::Task status;
// 			ssize_t bytes_read = read(req.pipe_fd, &status, sizeof(status));
// 			if (bytes_read > 0) {
// 				server.setTaskStatus(status, key);

// 				if (status == RequestState::COMPLETED) {
// 					close(req.pipe_fd);
// 					req.pipe_fd = -1;

// 					if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, nullptr) < 0) {
// 						//Logger::file("[ERROR] Failed to remove pipe_fd from epoll");
// 					}
// 				}
// 			}
// 		}

// 		switch (req.task) {
// 			case RequestState::IN_PROGRESS:
// 				++it;
// 				break;

// 			case RequestState::PENDING:
// 				++it;
// 				break;

// 			case RequestState::COMPLETED:
// 				++it;
// 				break;

// 			default:
// 				++it;
// 				break;
// 		}
// 	}
// }