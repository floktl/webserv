#include "TaskManager.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <iostream> // For debug statements

TaskManager::TaskManager(Server& _server) : server(_server)
{
	std::cout << "[DEBUG] TaskManager initialized" << std::endl;
}

std::string TaskManager::createTask()
{
	return ("hello");
}

TaskManager::Task TaskManager::getTaskStatus(const std::string& taskId)
{
	(void)taskId;
	TaskManager::Task task;
return (task);
}

void TaskManager::processTask(int epoll_fd)
{
	for (auto it = server.getGlobalFds().request_state_map.begin(); it != server.getGlobalFds().request_state_map.end(); )
	{
		(void)epoll_fd;
		int key = it->first;
		RequestState& req = it->second;

		if (req.pipe_fd != -1) {
			// Check if the status pipe is ready
			//struct epoll_event ev;
			//if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, req.pipe_fd, &ev) < 0) {
			//	Logger::file("[ERROR] Failed to modify epoll for pipe_fd");
			//	++it;
			//	continue;
			//}

			// Read status updates from the pipe
			int status;
			ssize_t bytes_read = read(req.pipe_fd, &status, sizeof(status));
			if (bytes_read > 0) {
				server.setTaskStatus(static_cast<RequestState::Task>(status), key);

				Logger::file("Request ID: " + std::to_string(key) + ", Task Status Updated: " +
					(status == RequestState::PENDING ? "PENDING" :
					status == RequestState::IN_PROGRESS ? "IN_PROGRESS" : "COMPLETED"));

				if (status == RequestState::COMPLETED) {
					close(req.pipe_fd);
					req.pipe_fd = -1;

					// Remove the pipe from epoll
					if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, nullptr) < 0) {
						Logger::file("[ERROR] Failed to remove pipe_fd from epoll");
					}
				}
			}
		}

		switch (req.task) {
			case RequestState::IN_PROGRESS:
				Logger::file("IN_PROGRESS - Handling CGI tasks");
				++it;
				break;

			case RequestState::PENDING:
				Logger::file("PENDING - Skipping");
				++it;
				break;

			case RequestState::COMPLETED:
				Logger::file("COMPLETED - Cleaning up");
				++it;
				break;

			default:
				++it;
				break;
		}
	}
}





void TaskManager::updateTaskStatus(Task& task)
{

	(void)task;
}
