#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP


#include "../main.hpp"
#include <mutex>
#include <thread>

class TaskManager
{
public:
	enum TaskStatus { PENDING, IN_PROGRESS, COMPLETED };

	struct Task {
		std::string id;
		TaskStatus status;
		int progress; // 0 to 100
	};
	TaskManager(Server& server);
	std::string createTask();
	Task getTaskStatus(const std::string& taskId);
	void processTask(const std::string& taskId);

private:
	std::map<std::string, Task> tasks;
	std::mutex taskMutex;
	Server &server;

	void updateTaskStatus(Task& task);
};

#endif
