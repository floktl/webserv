#include "RequestHandler.hpp"

#include <regex>

void RequestHandler::saveUploadedFile(const std::string &body, const std::string &path) {
    // Extract the filename from the Content-Disposition header
    std::regex filenameRegex(R"(Content-Disposition:.*filename=\"([^\"]+)\")");
    std::smatch matches;

    std::string filename;
    if (std::regex_search(body, matches, filenameRegex) && matches.size() > 1) {
        filename = matches[1].str(); // Extract the captured filename
    } else {
        std::cerr << "Failed to extract filename from the body" << std::endl;
        return;
    }

    // Locate the start of the actual file content in the body
    size_t contentStart = body.find("\r\n\r\n");
    if (contentStart == std::string::npos) {
        std::cerr << "Failed to find the start of the file content" << std::endl;
        return;
    }
    contentStart += 4; // Skip past the "\r\n\r\n" that separates headers from the content

    // Locate the boundary delimiter at the end of the file content
    size_t contentEnd = body.rfind("\r\n--");
    if (contentEnd == std::string::npos) {
        std::cerr << "Failed to find the end of the file content" << std::endl;
        return;
    }

    // Extract the file content from the body
    std::string fileContent = body.substr(contentStart, contentEnd - contentStart);

    // Construct the full file path
    std::string filePath = path + filename;

    // Write the content to the file
    std::ofstream outputFile(filePath, std::ios::binary);
    if (!outputFile) {
        std::cerr << "Failed to open file for writing: " << filePath << std::endl;
        return;
    }
    outputFile.write(fileContent.c_str(), fileContent.size());
    outputFile.close();

    std::cout << "File successfully saved to: " << filePath << std::endl;
}






void RequestHandler::parseRequest(RequestState &req)
{
    std::string request(req.request_buffer.begin(), req.request_buffer.end());

    // Find the end of the HTTP headers (indicated by "\r\n\r\n")
    size_t header_end = request.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return;

    // Extract the headers portion from the request
    std::string headers = request.substr(0, header_end);
    std::istringstream header_stream(headers);

    // Parse the request line
    std::string requestLine;
    std::getline(header_stream, requestLine);

    std::string method, path, version;
    std::istringstream request_stream(requestLine);
    request_stream >> method >> path >> version;

    // Check if the path contains a query string (indicated by '?')
    std::string query;
    size_t qpos = path.find('?');
    if (qpos != std::string::npos)
    {
        query = path.substr(qpos + 1);
        path = path.substr(0, qpos);
    }

    // Parse the headers for additional information (e.g., Content-Length)
    std::string line;
    size_t content_length = 0;
    while (std::getline(header_stream, line) && !line.empty())
    {
        if (line.rfind("Cookie:", 0) == 0)
        {
            std::string cookieValue = line.substr(std::strlen("Cookie: "));
            req.cookie_header = cookieValue;
        }
        else if (line.rfind("Content-Length:", 0) == 0)
        {
            content_length = std::stoul(line.substr(std::strlen("Content-Length: ")));
        }
    }

		// If it's a POST request, handle the body
	if (method == "POST")
	{
		Logger::file("DEBUG: Handling POST request for path: " + path);

		// Determine the body data (might be incomplete due to chunking)
		size_t body_start = header_end + 4;
		size_t received_body_length = request.size() - body_start;
		(void)received_body_length; // Avoid unused variable warning
		Logger::file("DEBUG: Chunk size received: " + std::to_string(received_body_length));

		if (req.received_body.empty())
		{
			// Reserve space for the expected body length
			req.received_body.reserve(content_length);
			Logger::file("DEBUG: Initialized received_body with expected content length: " + std::to_string(content_length));
		}

		// Append the current chunk to the received body
		req.received_body.append(request.substr(body_start));
		Logger::file("DEBUG: Appended received chunk. Total received body size: " + std::to_string(req.received_body.size()));

		// Check if the entire body has been received
		if (req.received_body.size() >= content_length)
		{
			Logger::file("INFO: Complete body received for POST request. Total size: " + std::to_string(req.received_body.size()));

			req.location_path = "uploads/";
			// Save the uploaded file
			saveUploadedFile(req.received_body, "/app/var/www/php/" + req.location_path);
			Logger::file("INFO: Uploaded file saved to path: " + req.location_path);

			// Update request state and epoll configuration
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			//server.delFromEpoll(server.getGlobalFds().epoll_fd, req.client_fd);
			req.state = RequestState::STATE_SENDING_RESPONSE;
			Logger::file("DEBUG: Request state updated to STATE_SENDING_RESPONSE. EPOLLOUT event set.");
		}
		else
		{

			// Log that we're waiting for more data
			Logger::file("DEBUG: Waiting for more data. Received: " + std::to_string(req.received_body.size()) +
						", Expected: " + std::to_string(content_length));
		}
		return;
	}



    // Standard request processing continues here for GET, PUT, etc.
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
