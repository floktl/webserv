// #ifndef TASKMANAGER_HPP
// #define TASKMANAGER_HPP


// #include "../main.hpp"
// #include <mutex>
// #include <thread>

// class TaskManager {
// public:
// 	struct Task {
// 		std::string id;
// 		RequestBody::Task status;
// 		int progress;
// 	};

// 	struct TaskUpdate {
// 		int client_fd;
// 		RequestBody::Task status;
// 	};
// 	TaskManager(Server& server);
// 	~TaskManager();

// 	int getPipeReadFd() const;

// 	std::string createTask();
// 	Task getTaskStatus(const std::string& taskId);
// 	void processTask(int epoll_fd = -1);
// 	void sendTaskStatusUpdate(int client_fd, RequestBody::Task status);

// private:
// 	std::map<std::string, Task> tasks;
// 	std::mutex taskMutex;
// 	Server& server;
// 	int pipe_fd[2];

// 	void handleTaskUpdates();
// };

// #endif