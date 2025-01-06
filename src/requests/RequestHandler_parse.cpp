#include "RequestHandler.hpp"

void RequestHandler::parseRequest(RequestState &req)
{
	std::string request(req.request_buffer.begin(), req.request_buffer.end());
	size_t header_end = request.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return;
	std::string headers = request.substr(0, header_end);
	std::istringstream header_stream(headers);
	std::string requestLine;
	std::getline(header_stream, requestLine);

	std::string method, path, version;
	std::istringstream request_stream(requestLine);
	request_stream >> method >> path >> version;

	std::string query;
	size_t qpos = path.find('?');
	if (qpos != std::string::npos)
	{
		query = path.substr(qpos + 1);
		path = path.substr(0, qpos);
	}

	std::string line;
	while (std::getline(header_stream, line) && !line.empty())
	{
		if (line.rfind("Cookie:", 0) == 0)
		{
			std::string cookieValue = line.substr(strlen("Cookie: "));
			req.cookie_header = cookieValue;
		}
	}

	if (method == "POST")
		req.request_body = request.substr(header_end + 4);

	req.location_path = path;

	std::stringstream redirectResponse;
	if (checkRedirect(req, &redirectResponse))
	{
		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
		return;
	}

	req.requested_path = buildRequestedPath(req, path);
	req.cgi_output_buffer.clear();

	if (server.getCgiHandler()->needsCGI(req, path))
	{
		req.state = RequestState::STATE_PREPARE_CGI;
		server.getCgiHandler()->addCgiTunnel(req, method, query);
	}
	else
	{
		buildResponse(req);
		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
	}
}
