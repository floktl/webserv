#include "ClientHandler.hpp"

ClientHandler::ClientHandler(Server& _server) : server(_server) {}

// void ClientHandler::handleClientRead(int epfd, int fd)
// {
//     auto &fds = server.getGlobalFds();
//     auto it = fds.request_state_map.find(fd);

//     if (it == fds.request_state_map.end())
//     {
//         //Logger::file("[handleClientRead] Invalid fd: " + std::to_string(fd) + " not found in request_state_map.");
//         server.delFromEpoll(epfd, fd);
//         return;
//     }

//     RequestState &req = it->second;
//     char buf[1024];

//     while (true)
//     {
//         ssize_t n = read(fd, buf, sizeof(buf));

//         if (n == 0)
//         {
//             //Logger::file("[handleClientRead] Client disconnected on fd: " + std::to_string(fd));
//             server.delFromEpoll(epfd, fd);
//             return;
//         }

//         if (n < 0)
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 break;
//             else
//             {
//                 //Logger::file("[handleClientRead] read error: "  + std::string(strerror(errno)));
//                 server.delFromEpoll(epfd, fd);
//                 return;
//             }
//         }

//         req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

//         try
//         {
//             server.getRequestHandler()->parseRequest(req);
//         }
//         catch (const std::exception &e)
//         {
//             //Logger::file("[handleClientRead] Error parsing request on fd: " + std::to_string(fd) + " - " + e.what());
//             server.delFromEpoll(epfd, fd);
//             return;
//         }

//     if (!req.clientMethodChecked) {

//         if (!processMethod(req, epfd))
// 		{
// 			req.clientMethodChecked = true;
// 		}
//     }
//         if (req.method == "POST" && req.received_body.size() < req.content_length)
//         {
//             //Logger::file("[handleClientRead] POST body incomplete. Waiting for more data. " + "Received: " + std::to_string(req.received_body.size()) + ", Expected: " + std::to_string(req.content_length));
//             server.modEpoll(epfd, fd, EPOLLIN);
//             break;
//         }
//     }
// }


constexpr std::chrono::seconds RequestState::TIMEOUT_DURATION;

// void ClientHandler::handleClientWrite(int epfd, int fd)
// {
//     RequestState &req = server.getGlobalFds().request_state_map[fd];

//     auto now = std::chrono::steady_clock::now();
//     if (now - req.last_activity > RequestState::TIMEOUT_DURATION)
//     {
//         //Logger::file("[handleClientWrite] Timeout reached for fd: "  + std::to_string(fd));
//         server.delFromEpoll(epfd, fd);
//         return;
//     }

//     if (req.state == RequestState::STATE_SENDING_RESPONSE)
//     {
//         int error = 0;
//         socklen_t len = sizeof(error);
//         if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
//         {
//             //Logger::file("[handleClientWrite] getsockopt error on fd: " + std::to_string(fd));
//             server.delFromEpoll(epfd, fd);
//             return;
//         }

//         if (!req.response_buffer.empty())
//         {
//             char send_buffer[8192];
//             size_t chunk_size = std::min(req.response_buffer.size(),
//                                          sizeof(send_buffer));

//             std::copy(req.response_buffer.begin(),
//                       req.response_buffer.begin() + chunk_size,
//                       send_buffer);

//             ssize_t n = send(fd, send_buffer, chunk_size, MSG_NOSIGNAL);

//             if (n > 0)
//             {
//                 req.response_buffer.erase(
//                     req.response_buffer.begin(),
//                     req.response_buffer.begin() + n
//                 );

//                 req.last_activity = now;

//                 if (req.response_buffer.empty())
//                 {
//                     //Logger::file("[handleClientWrite] Response fully sent. Removing fd: " + std::to_string(fd) + " from epoll.");
//                     server.delFromEpoll(epfd, fd);
//                 }
//                 else
//                 {
//                     ////Logger::file("[handleClientWrite] Partially sent response. " + "Still have " + std::to_string(req.response_buffer.size())+ " bytes left to send on fd: " + std::to_string(fd));
//                     server.modEpoll(epfd, fd, EPOLLOUT);
//                 }
//             }
//             else if (n < 0)
//             {
//                 if (errno != EAGAIN && errno != EWOULDBLOCK)
//                 {
//                     //Logger::file("[handleClientWrite] send() error on fd: " + std::to_string(fd) + ": " + std::string(strerror(errno)));
//                     server.delFromEpoll(epfd, fd);
//                 }
//                 else
//                 {
//                     // Wieder versuchen
//                     server.modEpoll(epfd, fd, EPOLLOUT);
//                 }
//             }
//         }
//     }
// }


// bool ClientHandler::processMethod(RequestState &req, int epoll_fd)
// {

// 	//Logger::file("[processMethod] Extracted method: " + req.method);

// 	if (!isMethodAllowed(req, req.method))
// 	{
// 		std::string error_response = server.getErrorHandler()->generateErrorResponse(405, "Method Not Allowed", req);

// 		if (error_response.length() > req.response_buffer.max_size())
// 		{
// 			error_response =
// 				"HTTP/1.1 405 Method Not Allowed\r\n"
// 				"Content-Type: text/plain\r\n"
// 				"Content-Length: 18\r\n"
// 				"Connection: close\r\n\r\n"
// 				"Method not allowed.";
// 		}

// 		req.response_buffer.clear();
// 		req.response_buffer.insert(req.response_buffer.begin(),
// 								error_response.begin(),
// 								error_response.end());

// 		req.state = RequestState::STATE_SENDING_RESPONSE;
// 		server.modEpoll(epoll_fd, req.client_fd, EPOLLOUT);
// 		return false;
// 	}

// 	return true;
// }

// bool ClientHandler::isMethodAllowed(const RequestState &req, const std::string &method) const {
// 	//Logger::file("[Method Check] Starting method validation for: " + method);
// 	//Logger::file("[Method Check] Request path: " + req.location_path);

// 	const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, req.location_path);

// 	if (!loc) {
// 		//Logger::file("[Method Check] ERROR: No matching location found for path: " + req.location_path);
// 		//Logger::file("[Method Check] Method check result: FALSE (no location)");
// 		return false;
// 	}

// 	//Logger::file("[Method Check] Found matching location");
// 	//Logger::file("[Method Check] Location config:");
// 	//Logger::file("  - GET allowed: " + std::string(loc->allowGet ? "yes" : "no"));
// 	//Logger::file("  - POST allowed: " + std::string(loc->allowPost ? "yes" : "no"));
// 	//Logger::file("  - DELETE allowed: " + std::string(loc->allowDelete ? "yes" : "no"));
// 	//Logger::file("  - COOKIE allowed: " + std::string(loc->allowCookie ? "yes" : "no"));

// 	bool isAllowed = false;
// 	std::string reason;

// 	if (method == "GET" && loc->allowGet) {
// 		isAllowed = true;
// 		reason = "GET method allowed for this location";
// 	}
// 	else if (method == "POST" && loc->allowPost) {
// 		isAllowed = true;
// 		reason = "POST method allowed for this location";
// 	}
// 	else if (method == "DELETE" && loc->allowDelete) {
// 		isAllowed = true;
// 		reason = "DELETE method allowed for this location";
// 	}
// 	else if (method == "COOKIE" && loc->allowCookie) {
// 		isAllowed = true;
// 		reason = "COOKIE method allowed for this location";
// 	}
// 	else {
// 		reason = method + " method not allowed for this location";
// 	}

// 	//Logger::file("[Method Check] Decision: " + std::string(isAllowed ? "ALLOWED" : "DENIED"));
// 	//Logger::file("[Method Check] Reason: " + reason);
// 	//Logger::file("[Method Check] ---- End of method check ----");

// 	return isAllowed;
// }