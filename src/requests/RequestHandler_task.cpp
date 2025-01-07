#include "RequestHandler.hpp"

void RequestHandler::handleTaskRequest(std::stringstream* response)
{
	TaskManager* taskManager = server.getTaskManager(); // Access TaskManager via Server
	std::string taskId = taskManager->createTask();

	// Manually build the 202 Accepted response with JSON-like content
	*response << "HTTP/1.1 202 Accepted\r\n";
	*response << "Content-Type: application/json\r\n\r\n";
	*response << "{\n"
			<< "  \"status\": \"in_progress\",\n"
			<< "  \"progress_url\": \"/api/task/" << taskId << "/status\"\n"
			<< "}";
}

void RequestHandler::handleStatusRequest(const std::string& taskId, std::stringstream* response)
{
	TaskManager* taskManager = server.getTaskManager();
	TaskManager::Task task = taskManager->getTaskStatus(taskId);

	// Determine task status as a string
	std::string status;
	switch (task.status)
	{
		case RequestState::PENDING:
			status = "pending";
			break;
		case RequestState::IN_PROGRESS:
			status = "in_progress";
			break;
		case RequestState::COMPLETED:
			status = "completed";
			break;
	}

	// Manually build the JSON-like response
	*response << "HTTP/1.1 200 OK\r\n";
	*response << "Content-Type: application/json\r\n\r\n";
	*response << "{\n"
			<< "  \"task_id\": \"" << task.id << "\",\n"
			<< "  \"status\": \"" << status << "\",\n"
			<< "  \"progress\": " << task.progress << "\n"
			<< "}";
}
