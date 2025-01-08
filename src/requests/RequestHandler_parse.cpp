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

    bool is_chunked = false;
    size_t content_length = 0;
    std::string line;

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
        else if (line.rfind("Transfer-Encoding:", 0) == 0)
        {
            std::string encoding = line.substr(std::strlen("Transfer-Encoding: "));
            if (encoding.find("chunked") != std::string::npos)
            {
                is_chunked = true;
            }
        }
    }

    LocationConfig* location = server.findMatchingLocation(path);
    if (!location)
    {
        Logger::file("ERROR: No matching location found for path: " + path);
        req.status_code = 404;
        req.state = RequestState::STATE_SENDING_RESPONSE;
        server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
        return;
    }

    if (method == "POST")
    {
        if (is_chunked)
        {
            size_t body_start = header_end + 4;
            std::string chunk_data = request.substr(body_start);

            if (req.chunked_state.processing)
            {
                req.chunked_state.buffer += chunk_data;
            }
            else
            {
                req.chunked_state.processing = true;
                req.chunked_state.buffer = chunk_data;
            }

            while (true)
            {
                size_t chunk_header_end = req.chunked_state.buffer.find("\r\n");
                if (chunk_header_end == std::string::npos)
                    return;

                std::string chunk_size_hex = req.chunked_state.buffer.substr(0, chunk_header_end);
                size_t chunk_size;
                std::stringstream ss;
                ss << std::hex << chunk_size_hex;
                ss >> chunk_size;

                if (req.chunked_state.buffer.size() < chunk_header_end + 2 + chunk_size + 2)
                    return;

                if (chunk_size == 0)
                {
                    req.chunked_state.processing = false;
                    if (req.received_body.size() > location->client_max_body_size)
                    {
                        Logger::file("ERROR: Chunked body exceeds client_max_body_size");
                        req.status_code = 413;
                        req.state = RequestState::STATE_SENDING_RESPONSE;
                        server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                        return;
                    }

                    req.location_path = "uploads/";
                    saveUploadedFile(req.received_body, "/app/var/www/php/" + req.location_path);
                    req.state = RequestState::STATE_SENDING_RESPONSE;
                    server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                    return;
                }

                std::string chunk = req.chunked_state.buffer.substr(
                    chunk_header_end + 2, chunk_size);

                if (req.received_body.size() + chunk.size() > location->client_max_body_size)
                {
                    Logger::file("ERROR: Chunked body would exceed client_max_body_size");
                    req.status_code = 413;
                    req.state = RequestState::STATE_SENDING_RESPONSE;
                    server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                    return;
                }

                req.received_body += chunk;
                req.chunked_state.buffer = req.chunked_state.buffer.substr(
                    chunk_header_end + 2 + chunk_size + 2);
            }
        }
        else
        {
            if (content_length > location->client_max_body_size)
            {
                Logger::file("ERROR: Content-Length " + std::to_string(content_length) +
                            " exceeds client_max_body_size " +
                            std::to_string(location->client_max_body_size));

                req.status_code = 413;
                req.state = RequestState::STATE_SENDING_RESPONSE;
                server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                return;
            }

            Logger::file("DEBUG: Handling POST request for path: " + path);

            size_t body_start = header_end + 4;
            size_t received_body_length = request.size() - body_start;
            Logger::file("DEBUG: Chunk size received: " + std::to_string(received_body_length));

            if (req.received_body.empty())
            {
                req.received_body.reserve(content_length);
                Logger::file("DEBUG: Initialized received_body with expected content length: " +
                            std::to_string(content_length));
            }

            if (req.received_body.size() + received_body_length > location->client_max_body_size)
            {
                Logger::file("ERROR: Accumulated body size would exceed client_max_body_size");
                req.status_code = 413;
                req.state = RequestState::STATE_SENDING_RESPONSE;
                server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                return;
            }

            req.received_body.append(request.substr(body_start));
            Logger::file("DEBUG: Appended received chunk. Total received body size: " +
                        std::to_string(req.received_body.size()));

            if (req.received_body.size() >= content_length)
            {
                if (req.received_body.size() == content_length)
                {
                    Logger::file("INFO: Complete body received for POST request. Total size: " +
                               std::to_string(req.received_body.size()));

                    req.location_path = "uploads/";
                    saveUploadedFile(req.received_body, "/app/var/www/php/" + req.location_path);
                    Logger::file("INFO: Uploaded file saved to path: " + req.location_path);

                    req.state = RequestState::STATE_SENDING_RESPONSE;
                    server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                }
                else
                {
                    Logger::file("ERROR: Received more data than specified in Content-Length");
                    req.status_code = 400;
                    req.state = RequestState::STATE_SENDING_RESPONSE;
                    server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
                }
            }
            return;
        }
    }

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