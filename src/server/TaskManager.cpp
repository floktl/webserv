#include "TaskManager.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <iostream> // For debug statements

TaskManager::TaskManager(Server& _server) : server(_server) {
	std::cout << "[DEBUG] TaskManager initialized" << std::endl;
}

std::string TaskManager::createTask() {
	std::lock_guard<std::mutex> lock(taskMutex);

	// Generate a unique task ID
	std::ostringstream idStream;
	idStream << std::chrono::steady_clock::now().time_since_epoch().count();
	std::string taskId = idStream.str();

	// Validate taskId length
	if (taskId.empty() || taskId.length() > 20) { // Arbitrary limit for safety
		std::cerr << "[ERROR] Generated taskId is invalid: " << taskId << std::endl;
		throw std::runtime_error("Invalid Task ID");
	}

	tasks[taskId] = {taskId, PENDING, 0};
	std::cout << "[DEBUG] Task created with ID: " << taskId << std::endl;

	// Start processing the task in a separate thread
	std::thread(&TaskManager::processTask, this, taskId).detach();
	std::cout << "[DEBUG] Task " << taskId << " processing started in a new thread" << std::endl;

	return taskId;
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

TaskManager::Task TaskManager::getTaskStatus(const std::string& taskId) {
	std::lock_guard<std::mutex> lock(taskMutex);
	auto it = tasks.find(taskId);
	if (it == tasks.end()) {
		std::cout << "[DEBUG] Task ID not found: " << taskId << std::endl;
		throw std::out_of_range("Task ID not found");
	}

	const Task& task = it->second;
	std::cout << "[DEBUG] Task status requested for ID: " << taskId
			<< " | Status: " << (task.status == PENDING ? "PENDING" :
								task.status == IN_PROGRESS ? "IN_PROGRESS" : "COMPLETED")
			<< " | Progress: " << task.progress << "%" << std::endl;

	return task;
}

void TaskManager::processTask(const std::string& taskId) {
	{
		std::lock_guard<std::mutex> lock(taskMutex);
		if (tasks.find(taskId) == tasks.end()) {
			std::cout << "[ERROR] Task ID not found: " << taskId << std::endl;
			return;
		}
		tasks[taskId].status = IN_PROGRESS;
		std::cout << "[DEBUG] Task " << taskId << " set to IN_PROGRESS" << std::endl;
	}

	for (int i = 1; i <= 100; ++i) {
		std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Simulate work

		{
			std::lock_guard<std::mutex> lock(taskMutex);
			// Ensure task ID is valid
			if (tasks.find(taskId) == tasks.end()) {
				std::cout << "[ERROR] Task ID disappeared during processing: " << taskId << std::endl;
				break;
			}

			if (i > 100) {
				std::cout << "[ERROR] Progress value exceeds limit: " << i << std::endl;
				break;
			}

			tasks[taskId].progress = i;
			std::cout << "[DEBUG] Task " << taskId << " progress updated to " << i << "%" << std::endl;
		}

		// Debug after releasing lock
		if (i % 10 == 0) {
			std::cout << "[DEBUG] Task " << taskId << " progress checkpoint: " << i << "%" << std::endl;
		}
	}

	{
		std::lock_guard<std::mutex> lock(taskMutex);

		// Final status check before marking complete
		if (tasks.find(taskId) == tasks.end()) {
			std::cout << "[ERROR] Task ID disappeared before completion: " << taskId << std::endl;
			return;
		}

		tasks[taskId].status = COMPLETED;
		tasks[taskId].progress = 100; // Ensure progress is capped at 100
		std::cout << "[DEBUG] Task " << taskId << " marked as COMPLETED" << std::endl;
	}

	std::cout << "[INFO] Task " << taskId << " fully processed." << std::endl;
}

void TaskManager::updateTaskStatus(Task& task)
{

void TaskManager::updateTaskStatus(Task& task) {
	// Simulate work progression
	if (task.status == PENDING) {
		task.status = IN_PROGRESS;
		std::cout << "[DEBUG] Task " << task.id << " status updated to IN_PROGRESS" << std::endl;
	}

	if (task.status == IN_PROGRESS) {
		// Update progress
		task.progress += 10; // Increment progress by 10% for simulation
		std::cout << "[DEBUG] Task " << task.id << " progress: " << task.progress << "%" << std::endl;

		// Mark as completed if progress reaches 100%
		if (task.progress >= 100) {
			task.progress = 100;
			task.status = COMPLETED;
			std::cout << "[DEBUG] Task " << task.id << " status updated to COMPLETED" << std::endl;
		}
	}
}
