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
                    std::string fileContent = request.substr(fileStart, fileEnd - fileStart);
                    outFile.write(fileContent.c_str(), fileContent.size());
                    outFile.close();
                }
            }
        }
    }

    std::string response = "HTTP/1.1 303 See Other\r\n";
    if (!req.cookie_header.empty())
        response += "Set-Cookie: " + req.cookie_header + "\r\n";
    response += "Location: /\r\n\r\n";
    req.response_buffer.assign(response.begin(), response.end());
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

    std::string response = "HTTP/1.1 204 No Content\r\n";
    if (!req.cookie_header.empty())
        response += "Set-Cookie: " + req.cookie_header + "\r\n";
    response += "\r\n";
    req.response_buffer.assign(response.begin(), response.end());
}

void RequestHandler::handleOtherRequests(RequestState &req, const Location* loc)
{
    std::stringstream buffer;
    std::stringstream response;

    struct stat path_stat;
    if (stat(req.requested_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
	{
        if (loc && loc->doAutoindex)
            buildAutoindexResponse(&response, req);
        else
            buildErrorResponse(404, "Directory listing not allowed", &response, req);
    }
	else
	{
        std::ifstream file(req.requested_path.c_str());
        if (file.is_open())
		{
            buffer << file.rdbuf();
            std::string file_content = buffer.str();
            file.close();

            response << "HTTP/1.1 200 OK\r\n";
            if (!req.cookie_header.empty())
                response << "Set-Cookie: " << req.cookie_header << "\r\n";
            response << "Content-Length: " << file_content.size() << "\r\n";
            response << "\r\n";
            response << file_content;
        }
		else
            buildErrorResponse(404, "File Not Found", &response, req);
    }

    req.response_buffer.assign(response.str().begin(), response.str().end());
}

void RequestHandler::buildErrorResponse(int statusCode, const std::string& message,
			std::stringstream *response, RequestState &req)
{
    *response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
			<< "Content-Type: text/html\r\n\r\n"
			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
	(void)req;
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
