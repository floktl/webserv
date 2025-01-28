#include "RequestHandler.hpp"
#include <fstream>
#include <iostream>

////void RequestHandler::buildResponse(RequestBody &req)
////{
////	const ServerBlock* conf = req.associated_conf;
////	if (!conf) return;

////	const Location* loc = findMatchingLocation(req.associated_conf, req.location_path);

////	if (req.ctx.method == "POST")	{
////		handlePostRequest(req);
////		return;
////	}

////	if (req.ctx.method == "DELETE")	{
////		handleDeleteRequest(req);
////		return;
////	}
////	handleOtherRequests(req, loc);
////}

//void RequestHandler::handlePostRequest(RequestBody &req)
//{
//	std::string request(req.request_buffer.begin(), req.request_buffer.end());

//	if (request.find("Content-Type: multipart/form-data") != std::string::npos)
//	{
//		size_t boundaryPos = request.find("boundary=");
//		if (boundaryPos != std::string::npos)
//		{
//			std::string boundary = "--" + request.substr(boundaryPos + 9);
//			boundary = boundary.substr(0, boundary.find("\r\n"));

//			size_t fileStart = request.find(boundary);
//			if (fileStart != std::string::npos)
//			{
//				fileStart = request.find("\r\n\r\n", fileStart) + 4;
//				size_t fileEnd = request.find(boundary, fileStart) - 2;

//				size_t fnameStart = request.rfind("filename=\"", fileStart);
//				size_t fnameEnd = request.find("\"", fnameStart + 10);
//				std::string filename = request.substr(fnameStart + 10, fnameEnd - (fnameStart + 10));

//				std::ofstream outFile(filename, std::ios::binary);
//				if (outFile.is_open())
//				{
//					const size_t chunk_size_post = 4096;
//					const char* data = request.data() + fileStart;
//					size_t remaining = fileEnd - fileStart;

//					while (remaining > 0) {
//						size_t write_size = std::min(remaining, chunk_size_post);
//						outFile.write(data, write_size);
//						data += write_size;
//						remaining -= write_size;
//					}
//					outFile.close();
//				}
//			}
//		}
//	}

//	std::string headers = "HTTP/1.1 303 See Other\r\n";
//	if (!req.cookie_header.empty())
//		headers += "Set-Cookie: " + req.cookie_header + "\r\n";
//	headers += "Location: /\r\n\r\n";

//	req.response_buffer.clear();

//	std::string headers_data = headers;
//	size_t chunk_size_headers = 64;
//	size_t total_size_headers = headers_data.size();

//	for (size_t i = 0; i < total_size_headers; i += chunk_size_headers) {
//		size_t length = std::min(chunk_size_headers, total_size_headers - i);
//		req.response_buffer.insert(req.response_buffer.end(),
//								headers_data.begin() + i,
//								headers_data.begin() + i + length);
//	}
//}

////void RequestHandler::handleDeleteRequest(RequestBody &req)
////{
////	////Logger::file("[handleDeleteRequest] Entering handleDeleteRequest for path: " + req.requested_path);

////	// Finde die passende Location für den aktuellen Request
////	const Location* loc = findMatchingLocation(req.associated_conf, req.location_path);
////	if (!loc)
////	{
////		////Logger::file("[handleDeleteRequest] No matching location found.");
////		std::stringstream response;
////		buildErrorResponse(404, "Not Found", &response, req);

////		std::string response_data = response.str();
////		// Füge "Connection: close" hinzu
////		response_data.insert(response_data.find("\r\n"), "Connection: close\r\n");

////		size_t chunk_size_delete = 64;
////		size_t total_size_delete = response_data.size();

////		req.response_buffer.clear();
////		for (size_t i = 0; i < total_size_delete; i += chunk_size_delete) {
////			size_t length = std::min(chunk_size_delete, total_size_delete - i);
////			req.response_buffer.insert(req.response_buffer.end(),
////										response_data.begin() + i,
////										response_data.begin() + i + length);
////		}
////		////Logger::file("[handleDeleteRequest] Sent 404 Not Found response");
////		// Setze den Zustand auf SENDING_RESPONSE
////		req.state = RequestBody::STATE_SENDING_RESPONSE;
////		server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
////		return;
////	}

//	// Verwende upload_store, um den Dateipfad zu konstruieren
//	std::string uploadStore = loc->upload_store;
//	if (uploadStore.empty())
//	{
//		////Logger::file("[handleDeleteRequest] upload_store is not defined for location: " + std::string(loc->path));
//		std::stringstream response;
//		buildErrorResponse(500, "Internal Server Error", &response, req);

//		std::string response_data = response.str();
//		// Füge "Connection: close" hinzu
//		response_data.insert(response_data.find("\r\n"), "Connection: close\r\n");

//		size_t chunk_size_delete_err = 64;
//		size_t total_size_delete_err = response_data.size();

//		req.response_buffer.clear();
//		for (size_t i = 0; i < total_size_delete_err; i += chunk_size_delete_err) {
//			size_t length = std::min(chunk_size_delete_err, total_size_delete_err - i);
//			req.response_buffer.insert(req.response_buffer.end(),
//										response_data.begin() + i,
//										response_data.begin() + i + length);
//		}
//		////Logger::file("[handleDeleteRequest] Sent 500 Internal Server Error response");
//		// Setze den Zustand auf SENDING_RESPONSE
//		req.state = RequestBody::STATE_SENDING_RESPONSE;
//		server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
//		return;
//	}

//	// Sicherstellen, dass uploadStore nicht mit '/' endet
//	if (!uploadStore.empty() && uploadStore.back() == '/')
//		uploadStore.pop_back();

//	// Extrahiere den Dateinamen aus dem Pfad
//	std::string filename = req.location_path;
//	std::string prefix = "/upload/";
//	if (filename.find(prefix) == 0)
//	{
//		filename = filename.substr(prefix.length());
//	}
//	else
//	{
//		////Logger::file("[handleDeleteRequest] Invalid path prefix: " + req.location_path);
//		std::stringstream response;
//		buildErrorResponse(400, "Bad Request", &response, req);

//		std::string response_data = response.str();
//		// Füge "Connection: close" hinzu
//		response_data.insert(response_data.find("\r\n"), "Connection: close\r\n");

//		size_t chunk_size_bad_request = 64;
//		size_t total_size_bad_request = response_data.size();

//		req.response_buffer.clear();
//		for (size_t i = 0; i < total_size_bad_request; i += chunk_size_bad_request) {
//			size_t length = std::min(chunk_size_bad_request, total_size_bad_request - i);
//			req.response_buffer.insert(req.response_buffer.end(),
//										response_data.begin() + i,
//										response_data.begin() + i + length);
//		}
//		////Logger::file("[handleDeleteRequest] Sent 400 Bad Request response");
//		// Setze den Zustand auf SENDING_RESPONSE
//		req.state = RequestBody::STATE_SENDING_RESPONSE;
//		server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
//		return;
//	}

//	std::string filePath = uploadStore + "/" + filename;

//	////Logger::file("[handleDeleteRequest] Constructed file path: " + filePath);

//	// Überprüfen, ob die Datei existiert
//	if (access(filePath.c_str(), F_OK) != 0)
//	{
//		////Logger::file("[handleDeleteRequest] File not found: " + filePath);
//		std::stringstream response;
//		buildErrorResponse(404, "Not Found", &response, req);

//		std::string response_data = response.str();
//		// Füge "Connection: close" hinzu
//		response_data.insert(response_data.find("\r\n"), "Connection: close\r\n");

//		size_t chunk_size_delete = 64;
//		size_t total_size_delete = response_data.size();

//		req.response_buffer.clear();
//		for (size_t i = 0; i < total_size_delete; i += chunk_size_delete) {
//			size_t length = std::min(chunk_size_delete, total_size_delete - i);
//			req.response_buffer.insert(req.response_buffer.end(),
//										response_data.begin() + i,
//										response_data.begin() + i + length);
//		}
//		////Logger::file("[handleDeleteRequest] Sent 404 Not Found response");
//		// Setze den Zustand auf SENDING_RESPONSE
//		req.state = RequestBody::STATE_SENDING_RESPONSE;
//		server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
//		return;
//	}

//	// Versuch, die Datei zu löschen
//	if (unlink(filePath.c_str()) != 0)
//	{
//		////Logger::file("[handleDeleteRequest] Failed to delete file: " + filePath + " Error: " + std::string(strerror(errno)));
//		std::stringstream response;
//		buildErrorResponse(500, "Internal Server Error", &response, req);

//		std::string response_data = response.str();
//		// Füge "Connection: close" hinzu
//		response_data.insert(response_data.find("\r\n"), "Connection: close\r\n");

//		size_t chunk_size_delete_err = 64;
//		size_t total_size_delete_err = response_data.size();

//		req.response_buffer.clear();
//		for (size_t i = 0; i < total_size_delete_err; i += chunk_size_delete_err) {
//			size_t length = std::min(chunk_size_delete_err, total_size_delete_err - i);
//			req.response_buffer.insert(req.response_buffer.end(),
//										response_data.begin() + i,
//										response_data.begin() + i + length);
//		}
//		////Logger::file("[handleDeleteRequest] Sent 500 Internal Server Error response");
//		// Setze den Zustand auf SENDING_RESPONSE
//		req.state = RequestBody::STATE_SENDING_RESPONSE;
//		server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
//		return;
//	}

//	////Logger::file("[handleDeleteRequest] Successfully deleted file: " + filePath);

//	// Senden der 204 No Content Antwort mit "Connection: close"
//	std::string headers = "HTTP/1.1 204 No Content\r\n";
//	if (!req.cookie_header.empty())
//		headers += "Set-Cookie: " + req.cookie_header + "\r\n";
//	headers += "Connection: close\r\n";
//	headers += "\r\n";

//	req.response_buffer.clear();

//	std::string headers_data = headers;
//	size_t chunk_size_no_content = 64;
//	size_t total_size_no_content = headers_data.size();

//	for (size_t i = 0; i < total_size_no_content; i += chunk_size_no_content) {
//		size_t length = std::min(chunk_size_no_content, total_size_no_content - i);
//		req.response_buffer.insert(req.response_buffer.end(),
//									headers_data.begin() + i,
//									headers_data.begin() + i + length);
//	}

//	////Logger::file("[handleDeleteRequest] Sent 204 No Content response");

//	// Setze den Zustand auf SENDING_RESPONSE
//	req.state = RequestBody::STATE_SENDING_RESPONSE;
//	server.modEpoll(server.getGlobalFds().epoll_fd, req.ctx.client_fd, EPOLLOUT);
//}

//void RequestHandler::handleOtherRequests(RequestBody &req, const Location* loc)
//{
//	struct stat path_stat;
//	if (stat(req.requested_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode))
//	{
//		std::stringstream response;
//		if (loc && loc->doAutoindex)
//			buildAutoindexResponse(&response, req);
//		else
//			buildErrorResponse(404, "Not found", &response, req);

//		std::string response_data = response.str();
//		size_t chunk_size_dir = 64;
//		size_t total_size_dir = response_data.size();

//		req.response_buffer.clear();
//		for (size_t i = 0; i < total_size_dir; i += chunk_size_dir) {
//			size_t length = std::min(chunk_size_dir, total_size_dir - i);
//			req.response_buffer.insert(req.response_buffer.end(),
//									response_data.begin() + i,
//									response_data.begin() + i + length);
//		}
//	}
//	else
//	{
//		std::ifstream file(req.requested_path.c_str(), std::ios::binary);
//		if (file.is_open())
//		{
//			std::string headers = "HTTP/1.1 200 OK\r\n";
//			if (!req.cookie_header.empty())
//				headers += "Set-Cookie: " + req.cookie_header + "\r\n";

//			file.seekg(0, std::ios::end);
//			std::streamsize file_size = file.tellg();
//			file.seekg(0, std::ios::beg);

//			headers += "Content-Length: " + std::to_string(file_size) + "\r\n\r\n";

//			req.response_buffer.clear();

//			std::string headers_data = headers;
//			size_t chunk_size_headers_file = 64;
//			size_t total_size_headers_file = headers_data.size();

//			for (size_t i = 0; i < total_size_headers_file; i += chunk_size_headers_file) {
//				size_t length = std::min(chunk_size_headers_file, total_size_headers_file - i);
//				req.response_buffer.insert(req.response_buffer.end(),
//										headers_data.begin() + i,
//										headers_data.begin() + i + length);
//			}

//			const size_t file_chunk_size = 4096;
//			char chunk[file_chunk_size];

//			while (!file.eof() && file.good())
//			{
//				file.read(chunk, file_chunk_size);
//				std::streamsize bytes_read = file.gcount();
//				if (bytes_read > 0)
//				{
//					if (req.response_buffer.size() + bytes_read > req.response_buffer.max_size())
//					{
//						file.close();
//						std::stringstream error_stream;
//						buildErrorResponse(500, "Response too large", &error_stream, req);

//						std::string response_data = error_stream.str();
//						size_t chunk_size_large_response = 64;
//						size_t total_size_large_response = response_data.size();

//						req.response_buffer.clear();
//						for (size_t i = 0; i < total_size_large_response; i += chunk_size_large_response) {
//							size_t length = std::min(chunk_size_large_response, total_size_large_response - i);
//							req.response_buffer.insert(req.response_buffer.end(),
//													response_data.begin() + i,
//													response_data.begin() + i + length);
//						}
//						return;
//					}

//					req.response_buffer.insert(req.response_buffer.end(), chunk, chunk + bytes_read);
//				}
//			}
//			file.close();
//		}
//		else
//		{
//			std::stringstream response;
//			buildErrorResponse(404, "File Not Found", &response, req);

//			std::string response_data = response.str();
//			size_t chunk_size_file_not_found = 64;
//			size_t total_size_file_not_found = response_data.size();

//			req.response_buffer.clear();
//			for (size_t i = 0; i < total_size_file_not_found; i += chunk_size_file_not_found) {
//				size_t length = std::min(chunk_size_file_not_found, total_size_file_not_found - i);
//				req.response_buffer.insert(req.response_buffer.end(),
//										response_data.begin() + i,
//										response_data.begin() + i + length);
//			}
//		}
//	}
//}

//void RequestHandler::buildErrorResponse(int statusCode, const std::string& message,
//									std::stringstream* response, RequestBody &req)
//{
//	std::stringstream error_response;
//	error_response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
//				<< "Content-Type: text/html\r\n";

//	if (!req.cookie_header.empty())
//		error_response << "Set-Cookie: " << req.cookie_header << "\r\n";

//	std::string html_content = "<html><body><h1>" + std::to_string(statusCode) + " " + message + "</h1></body></html>";
//	error_response << "Content-Length: " << html_content.length() << "\r\n\r\n"
//				<< html_content;

//	*response = std::move(error_response);
//}

//void RequestHandler::buildAutoindexResponse(std::stringstream* response, RequestBody& req)
//{
//	if (!checkDirectoryPermissions(req, response))
//		return;

//	std::vector<DirEntry> entries = getDirectoryEntries(req, response);
//	if (entries.empty())
//		return;

//	sortDirectoryEntries(entries);
//	generateAutoindexHtml(response, req, entries);
//}
