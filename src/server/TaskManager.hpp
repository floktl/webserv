#ifndef TASKMANAGER_HPP
#define TASKMANAGER_HPP


#include "../main.hpp"
#include <mutex>
#include <thread>

class TaskManager {
public:
    struct Task {
        std::string status;
        int progress;
        std::chrono::steady_clock::time_point last_update;
    };

private:
    std::map<std::string, Task> tasks;
    std::mutex mutex;

public:

	void cleanUpTasks(std::chrono::minutes timeout) {
		auto now = std::chrono::steady_clock::now();
		std::lock_guard<std::mutex> lock(mutex);
		for (auto it = tasks.begin(); it != tasks.end();) {
			if (it->second.status == "completed" &&
				std::chrono::duration_cast<std::chrono::minutes>(now - it->second.last_update) >= timeout) {
				it = tasks.erase(it);
			} else {
				++it;
			}
		}
	}

    std::string createTask() {
        static int task_id_counter = 0;
        std::string task_id = "task_" + std::to_string(++task_id_counter);

        std::lock_guard<std::mutex> lock(mutex);
        tasks[task_id] = {"in_progress", 0, std::chrono::steady_clock::now()};

        // Launch a background thread to process the task
        std::thread(&TaskManager::simulateTaskProgress, this, task_id).detach();

        return task_id;
    }

    Task getTask(const std::string& task_id) {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = tasks.find(task_id);
        if (it != tasks.end()) {
            return it->second;
        }
        return {"not_found", 0, std::chrono::steady_clock::now()};
    }

private:
    void simulateTaskProgress(const std::string& task_id) {
        for (int i = 0; i <= 100; i += 20) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                tasks[task_id].progress = i;
                tasks[task_id].status = (i < 100) ? "in_progress" : "completed";
                tasks[task_id].last_update = std::chrono::steady_clock::now();
            }
			std::cout << "maeaehhh" << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }
};

#endif
