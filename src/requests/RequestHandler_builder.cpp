#include "RequestHandler.hpp"
#include <fstream>
#include <iostream>

void RequestHandler::buildResponse(RequestState &req)
{
    const ServerBlock* conf = req.associated_conf;
    if (!conf) return;

    const Location* loc = findMatchingLocation(req.associated_conf, req.location_path);
    std::string method = getMethod(req.request_buffer);

    if (method == "POST")
	{
        handlePostRequest(req);
        return;
    }

    if (method == "DELETE")
	{
        handleDeleteRequest(req);
        return;
    }

    handleOtherRequests(req, loc);
}

void RequestHandler::handlePostRequest(RequestState &req)
{
    std::string request(req.request_buffer.begin(), req.request_buffer.end());

    if (request.find("Content-Type: multipart/form-data") != std::string::npos)
    {
        size_t boundaryPos = request.find("boundary=");
        if (boundaryPos != std::string::npos)
        {
            std::string boundary = "--" + request.substr(boundaryPos + 9);
            boundary = boundary.substr(0, boundary.find("\r\n"));

            size_t fileStart = request.find(boundary);
            if (fileStart != std::string::npos)
            {
                fileStart = request.find("\r\n\r\n", fileStart) + 4;
                size_t fileEnd = request.find(boundary, fileStart) - 2;

                size_t fnameStart = request.rfind("filename=\"", fileStart);
                size_t fnameEnd = request.find("\"", fnameStart + 10);
                std::string filename = request.substr(fnameStart + 10, fnameEnd - (fnameStart + 10));

                std::ofstream outFile(filename, std::ios::binary);
                if (outFile.is_open())
                {
                    const size_t chunk_size = 4096;
                    const char* data = request.data() + fileStart;
                    size_t remaining = fileEnd - fileStart;

                    while (remaining > 0) {
                        size_t write_size = std::min(remaining, chunk_size);
                        outFile.write(data, write_size);
                        data += write_size;
                        remaining -= write_size;
                    }
                    outFile.close();
                }
            }
        }
    }

    std::string headers = "HTTP/1.1 303 See Other\r\n";
    if (!req.cookie_header.empty())
        headers += "Set-Cookie: " + req.cookie_header + "\r\n";
    headers += "Location: /\r\n\r\n";

    req.response_buffer.clear();
    if (headers.length() <= req.response_buffer.max_size()) {
        req.response_buffer.insert(req.response_buffer.end(), headers.begin(), headers.end());
    } else {
        std::stringstream error_stream;
        buildErrorResponse(500, "Response too large", &error_stream, req);
        req.response_buffer.assign(error_stream.str().begin(), error_stream.str().end());
    }
}

void RequestHandler::handleDeleteRequest(RequestState &req)
{
    if (access(req.requested_path.c_str(), F_OK) != 0)
    {
        std::stringstream response;
        buildErrorResponse(404, "Not Found", &response, req);
        req.response_buffer.assign(response.str().begin(), response.str().end());
        return;
    }

    if (unlink(req.requested_path.c_str()) != 0)
    {
        std::stringstream response;
        buildErrorResponse(500, "Internal Server Error", &response, req);
        req.response_buffer.assign(response.str().begin(), response.str().end());
        return;
    }

    std::string headers = "HTTP/1.1 204 No Content\r\n";
    if (!req.cookie_header.empty())
        headers += "Set-Cookie: " + req.cookie_header + "\r\n";
    headers += "\r\n";

    req.response_buffer.clear();
    if (headers.length() <= req.response_buffer.max_size()) {
        req.response_buffer.insert(req.response_buffer.end(), headers.begin(), headers.end());
    } else {
        std::stringstream error_stream;
        buildErrorResponse(500, "Response too large", &error_stream, req);
        req.response_buffer.assign(error_stream.str().begin(), error_stream.str().end());
    }
}

void RequestHandler::handleOtherRequests(RequestState &req, const Location* loc)
{
    struct stat path_stat;
    if (stat(req.requested_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
    {
        std::stringstream response;
        if (loc && loc->doAutoindex)
            buildAutoindexResponse(&response, req);
        else
            buildErrorResponse(404, "Directory listing not allowed", &response, req);
        req.response_buffer.assign(response.str().begin(), response.str().end());
    }
    else
    {
        std::ifstream file(req.requested_path.c_str(), std::ios::binary);
        if (file.is_open())
        {
            std::string headers = "HTTP/1.1 200 OK\r\n";
            if (!req.cookie_header.empty())
                headers += "Set-Cookie: " + req.cookie_header + "\r\n";

            file.seekg(0, std::ios::end);
            std::streamsize file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            headers += "Content-Length: " + std::to_string(file_size) + "\r\n\r\n";

            req.response_buffer.clear();
            req.response_buffer.insert(req.response_buffer.end(), headers.begin(), headers.end());

            const size_t chunk_size = 4096;
            char chunk[chunk_size];

            while (!file.eof() && file.good())
            {
                file.read(chunk, chunk_size);
                std::streamsize bytes_read = file.gcount();
                if (bytes_read > 0)
                {
                    if (req.response_buffer.size() + bytes_read > req.response_buffer.max_size())
                    {
                        file.close();
                        std::stringstream error_stream;
                        buildErrorResponse(500, "Response too large", &error_stream, req);
                        req.response_buffer.assign(error_stream.str().begin(), error_stream.str().end());
                        return;
                    }
                    req.response_buffer.insert(req.response_buffer.end(), chunk, chunk + bytes_read);
                }
            }
            file.close();
        }
        else
        {
            std::stringstream response;
            buildErrorResponse(404, "File Not Found", &response, req);
            req.response_buffer.assign(response.str().begin(), response.str().end());
        }
    }
}

void RequestHandler::buildErrorResponse(int statusCode, const std::string& message,
                                      std::stringstream* response, RequestState &req)
{
    std::stringstream error_response;
    error_response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
                  << "Content-Type: text/html\r\n";

    if (!req.cookie_header.empty())
        error_response << "Set-Cookie: " << req.cookie_header << "\r\n";

    std::string html_content = "<html><body><h1>" + std::to_string(statusCode) + " " + message + "</h1></body></html>";
    error_response << "Content-Length: " << html_content.length() << "\r\n\r\n"
                  << html_content;

    *response = std::move(error_response);
}

void RequestHandler::buildAutoindexResponse(std::stringstream* response, RequestState& req)
{
    if (!checkDirectoryPermissions(req, response))
        return;

    std::vector<DirEntry> entries = getDirectoryEntries(req, response);
    if (entries.empty())
        return;

    sortDirectoryEntries(entries);
    generateAutoindexHtml(response, req, entries);
}
