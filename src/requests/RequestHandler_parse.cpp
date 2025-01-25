#include "RequestHandler.hpp"

#include <regex>

bool RequestHandler::handleChunkedUpload(RequestState &req, const std::string &request, size_t headerEnd, const Location* loc)
{
	size_t bodyStart = headerEnd + 4;
	std::string chunkData = request.substr(bodyStart);

	if (!req.chunked_state.processing)
	{
		req.chunked_state.processing = true;
	}
	req.chunked_state.buffer += chunkData;

	while (true)
	{
		size_t chunkHeaderEnd = req.chunked_state.buffer.find("\r\n");
		if (chunkHeaderEnd == std::string::npos)
			return false;

		std::string chunkSizeHex = req.chunked_state.buffer.substr(0, chunkHeaderEnd);
		size_t chunkSize = 0;
		{
			std::stringstream ss;
			ss << std::hex << chunkSizeHex;
			ss >> chunkSize;
		}

		if (req.chunked_state.buffer.size() < chunkHeaderEnd + 2 + chunkSize + 2)
			return false;

		if (chunkSize == 0)
		{
			req.chunked_state.processing = false;

			std::string uploadPath = loc->upload_store;
			if (!uploadPath.empty() && uploadPath.back() != '/')
				uploadPath.push_back('/');
			uploadPath += "chunked_upload.dat";

			std::ofstream ofs(uploadPath, std::ios::binary);
			ofs.write(req.received_body.c_str(), req.received_body.size());
			ofs.close();

			return true;
		}

		std::string chunk = req.chunked_state.buffer.substr(chunkHeaderEnd + 2, chunkSize);
		req.received_body += chunk;

		req.chunked_state.buffer.erase(0, chunkHeaderEnd + 2 + chunkSize + 2);

		if (req.received_body.size() > (size_t)loc->client_max_body_size)
		{
			return false;
		}
	}
}

void RequestHandler::saveUploadedFile(const std::string &body, const std::string &path) {
	Logger::file("[Upload] Starting file upload process");

	// Extract filename
	std::regex filenameRegex(R"(Content-Disposition:.*filename=\"([^\"]+)\")");
	std::smatch matches;

	std::string filename;
	if (std::regex_search(body, matches, filenameRegex) && matches.size() > 1) {
		filename = matches[1].str();
		Logger::file("[Upload] Extracted filename: " + filename);
	} else {
		Logger::file("[Upload] ERROR: Failed to extract filename from body");
		return;
	}

	// Find content boundaries
	size_t contentStart = body.find("\r\n\r\n");
	if (contentStart == std::string::npos) {
		Logger::file("[Upload] ERROR: Failed to find content start boundary");
		return;
	}
	contentStart += 4;
	Logger::file("[Upload] Content starts at position: " + std::to_string(contentStart));

	size_t contentEnd = body.rfind("\r\n--");
	if (contentEnd == std::string::npos) {
		Logger::file("[Upload] ERROR: Failed to find content end boundary");
		return;
	}
	Logger::file("[Upload] Content ends at position: " + std::to_string(contentEnd));

	// Extract content
	std::string fileContent = body.substr(contentStart, contentEnd - contentStart);
	Logger::file("[Upload] Extracted file content size: " + std::to_string(fileContent.size()));

	// Build full path and save
	std::string filePath = path + filename;
	Logger::file("[Upload] Saving to path: " + filePath);

	std::ofstream outputFile(filePath, std::ios::binary);
	if (!outputFile) {
		Logger::file("[Upload] ERROR: Failed to open output file: " + filePath);
		return;
	}

	outputFile.write(fileContent.c_str(), fileContent.size());
	outputFile.close();

	if (outputFile) {
		Logger::file("[Upload] File successfully saved");

		std::string successResponse = "HTTP/1.1 200 OK\r\n"
									"Content-Type: text/plain\r\n"
									"Content-Length: 13\r\n"
									"\r\n"
									"Upload success";

		Logger::file("[Upload] Success response prepared");
	} else {
		Logger::file("[Upload] ERROR: Failed to write file");
	}
}



void RequestHandler::finalizeUpload(RequestState &req) {
	Logger::file("[Upload] Finalizing upload process");

	std::string successResponse = "HTTP/1.1 200 OK\r\n"
								"Content-Type: text/plain\r\n"
								"Content-Length: 13\r\n"
								"\r\n"
								"Upload success";

	req.response_buffer.clear();
	req.response_buffer.insert(req.response_buffer.end(),
							successResponse.begin(),
							successResponse.end());

	req.state = RequestState::STATE_SENDING_RESPONSE;

	server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);

	Logger::file("[Upload] Upload finalized, response ready to send");
}

bool detectMultipartFormData(RequestState &req)
{
    std::string bufferStr(req.request_buffer.begin(), req.request_buffer.end());

    if (bufferStr.find("Content-Type: multipart/form-data") != std::string::npos)
    {
        req.is_multipart = true;  // Assign boolean flag to RequestState
        Logger::file("[detectMultipartFormData] Multipart form-data detected in request.");
        return true;
    }

    req.is_multipart = false;
    Logger::file("[detectMultipartFormData] No multipart form-data detected.");
    return false;
}

void RequestHandler::parseRequest(RequestState &req)
{
	Logger::file("[parseRequest] Entering parseRequest. ParsingPhase = "
				+ std::to_string(req.parsing_phase)
				+ ", request_buffer size: "
				+ std::to_string(req.request_buffer.size()));

	if (req.parsing_phase == RequestState::PARSING_HEADER)
	{
		std::string bufferContent(req.request_buffer.begin(), req.request_buffer.end());
		size_t headerEndPos = bufferContent.find("\r\n\r\n");
		if (headerEndPos == std::string::npos)
		{
			Logger::file("[parseRequest] Header not complete -> waiting for more data");
			return;
		}

		std::string headers = bufferContent.substr(0, headerEndPos);
		Logger::file("[parseRequest] Found header boundary at: " + std::to_string(headerEndPos));
		Logger::file("[parseRequest] Headers:\n" + headers);

		std::istringstream headerStream(headers);
		std::string requestLine;
		std::getline(headerStream, requestLine);

		if (requestLine.empty())
		{
			Logger::file("[parseRequest] Request line is empty -> Something's off. Waiting...");
			return;
		}

		std::string method, path, version;
		{
			std::istringstream rlstream(requestLine);
			rlstream >> method >> path >> version;
			if (path.empty()) path = "/";
		}

		Logger::file("[parseRequest] Parsed Method: '" + method
					+ "', Path: '" + path
					+ "', Version: '" + version + "'");

		static const std::set<std::string> validMethods = {
			"GET", "POST", "DELETE", "HEAD", "PUT", "OPTIONS", "PATCH"
		};
		if (validMethods.find(method) == validMethods.end())
		{
			Logger::file("[parseRequest] Unknown or incomplete method '"
						+ method + "' -> waiting for more data");
			return;
		}

		std::string query;
		size_t qpos = path.find('?');
		if (qpos != std::string::npos)
		{
			query = path.substr(qpos + 1);
			path  = path.substr(0, qpos);
		}

		bool   is_chunked      = false;
		size_t content_length  = 0;
		std::string line;
		while (std::getline(headerStream, line) && !line.empty())
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			Logger::file("[parseRequest] Header line: " + line);

			if (line.rfind("Cookie:", 0) == 0)
			{
				req.cookie_header = line.substr(std::strlen("Cookie: "));
			}
			else if (line.rfind("Content-Length:", 0) == 0)
			{
				content_length = static_cast<size_t>(
					std::stoul(line.substr(std::strlen("Content-Length: ")))
				);
				Logger::file("[parseRequest] Found Content-Length: "
							+ std::to_string(content_length));
			}
			else if (line.rfind("Transfer-Encoding:", 0) == 0)
			{
				std::string encoding = line.substr(std::strlen("Transfer-Encoding: "));
				if (encoding.find("chunked") != std::string::npos)
				{
					Logger::file("[parseRequest] Found chunked Transfer-Encoding");
					is_chunked = true;
				}
			}
		}

		const Location* location = findMatchingLocation(req.associated_conf, path);
		if (!location)
		{
			Logger::file("[parseRequest] No matching location -> sending 404");
			std::stringstream errorStream;
			buildErrorResponse(404, "Not Found", &errorStream, req);
			std::string err = errorStream.str();
			req.response_buffer.clear();
			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
			req.state = RequestState::STATE_SENDING_RESPONSE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}

		size_t bodyPos = headerEndPos + 4;
		std::string bodyPart;
		if (bufferContent.size() > bodyPos)
			bodyPart = bufferContent.substr(bodyPos);

		req.method         = method;
		req.location_path  = path;
		req.requested_path = buildRequestedPath(req, path);
		req.content_length = content_length;
		req.received_body  = bodyPart;
		detectMultipartFormData(req);
		printRequestState(req);
		req.request_buffer.clear();

		std::stringstream redirectResponse;
		if (checkRedirect(req, &redirectResponse))
		{
			Logger::file("[parseRequest] checkRedirect -> redirect response set");
			req.state = RequestState::STATE_SENDING_RESPONSE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}

		// Spezifische Behandlung der HTTP-Methoden
		if (method == "POST")
		{
			Logger::file("[parseRequest] It's a POST request");
			if (is_chunked)
			{
				Logger::file("[parseRequest] Handling chunked upload...");
				if (!handleChunkedUpload(req, bufferContent, headerEndPos, location))
				{
					Logger::file("[parseRequest] Not all chunks available yet -> waiting");
					return;
				}
				req.parsing_phase = RequestState::PARSING_COMPLETE;
				return;
			}

			if (content_length > (size_t)location->client_max_body_size)
			{
				Logger::file("[parseRequest] 413 Payload Too Large");
				std::stringstream errorStream;
				buildErrorResponse(413, "Payload Too Large", &errorStream, req);
				std::string err = errorStream.str();
				req.response_buffer.clear();
				req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
				req.state = RequestState::STATE_SENDING_RESPONSE;
				server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
				return;
			}

			if (req.received_body.size() >= content_length)
			{
				Logger::file("[parseRequest] Body fully in first chunk ("
							+ std::to_string(req.received_body.size()) + " bytes)");

				if (server.getCgiHandler()->needsCGI(req, req.location_path))
				{
					Logger::file("[parseRequest] It's a CGI request => setting up CGI");
					req.state = RequestState::STATE_PREPARE_CGI;
					server.getCgiHandler()->addCgiTunnel(req, req.method, /* query */ "");
					Logger::file("[parseRequest] CGI setup done, returning");
					req.parsing_phase = RequestState::PARSING_COMPLETE;
					return;
				}
				else
				{
					Logger::file("[parseRequest] It's a normal (non-CGI) upload => finalizing file");
					saveUploadedFile(req.received_body, location->upload_store + "/");
					finalizeUpload(req);
					req.parsing_phase = RequestState::PARSING_COMPLETE;
					return;
				}
			}
			else
			{
				Logger::file("[parseRequest] Body incomplete => switching to PARSING_BODY");
				req.parsing_phase = RequestState::PARSING_BODY;
				return;
			}
		}
		else if (method == "DELETE")
		{
			Logger::file("[parseRequest] It's a DELETE request");
			handleDeleteRequest(req);
			req.parsing_phase = RequestState::PARSING_COMPLETE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}
		else
		{
			Logger::file("[parseRequest] Handling method: " + method);

			if (server.getCgiHandler()->needsCGI(req, path))
			{
				Logger::file("[parseRequest] It's a CGI request => setting up CGI");
				req.state = RequestState::STATE_PREPARE_CGI;
				server.getCgiHandler()->addCgiTunnel(req, method, query);
			}
			else
			{
				Logger::file("[parseRequest] buildResponse for normal request");
				buildResponse(req);
				req.state = RequestState::STATE_SENDING_RESPONSE;
			}
			req.parsing_phase = RequestState::PARSING_COMPLETE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}
	}

	else if (req.parsing_phase == RequestState::PARSING_BODY)
	{
		Logger::file("[parseRequest] PARSING_BODY -> collecting leftover body bytes");

		if (!req.request_buffer.empty())
		{
			req.received_body.insert(req.received_body.end(),
									req.request_buffer.begin(),
									req.request_buffer.end());
			req.request_buffer.clear();
		}

		const Location* location = findMatchingLocation(req.associated_conf, req.location_path);
		if (!location)
		{
			Logger::file("[parseRequest] Lost location? -> 404");
			std::stringstream errorStream;
			buildErrorResponse(404, "Not Found", &errorStream, req);
			std::string err = errorStream.str();
			req.response_buffer.clear();
			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
			req.state = RequestState::STATE_SENDING_RESPONSE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}

		if (req.received_body.size() > (size_t)location->client_max_body_size)
		{
			Logger::file("[parseRequest] 413 Payload Too Large while reading body");
			std::stringstream errorStream;
			buildErrorResponse(413, "Payload Too Large", &errorStream, req);
			std::string err = errorStream.str();
			req.response_buffer.clear();
			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
			req.state = RequestState::STATE_SENDING_RESPONSE;
			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
			return;
		}

		if (req.received_body.size() >= req.content_length)
		{
			Logger::file("[parseRequest] Body is now complete => deciding next steps");

			if (server.getCgiHandler()->needsCGI(req, req.location_path))
			{
				Logger::file("[parseRequest] It's a CGI request => setting up CGI");
				req.state = RequestState::STATE_PREPARE_CGI;
				server.getCgiHandler()->addCgiTunnel(req, req.method, /* query */ "");
				Logger::file("[parseRequest] CGI setup done, returning");
				req.parsing_phase = RequestState::PARSING_COMPLETE;
				return;
			}
			else
			{
				Logger::file("[parseRequest] It's a normal (non-CGI) upload => finalizing file");
				saveUploadedFile(req.received_body, location->upload_store + "/");
				finalizeUpload(req);
				req.parsing_phase = RequestState::PARSING_COMPLETE;
				return;
			}
		}
		else
		{
			Logger::file("[parseRequest] Still not enough body data -> waiting for more");
			return;
		}
	}
	else if (req.parsing_phase == RequestState::PARSING_COMPLETE)
	{
		Logger::file("[parseRequest] PARSING_COMPLETE -> Doing nothing, request is done");
		return;
	}
}
