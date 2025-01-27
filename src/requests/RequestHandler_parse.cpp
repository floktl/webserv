// #include "RequestHandler.hpp"

// #include <regex>
// void RequestHandler::handlePostBodyComplete(RequestBody &req, const Location* location)
// {
// 	//Logger::file("[handlePostBodyComplete] POST-Body komplett empfangen.");

// 	// Falls CGI notwendig:
// 	if (server.getCgiHandler()->needsCGI(req, req.location_path))
// 	{
// 		//Logger::file("[handlePostBodyComplete] CGI wird benötigt -> CGI vorbereiten.");
// 		req.state = RequestBody::STATE_PREPARE_CGI;
// 		server.getCgiHandler()->addCgiTunnel(req, req.method, /* query */ "");
// 		return;
// 	}
// 	else
// 	{
// 		//Logger::file("[handlePostBodyComplete] Kein CGI -> Datei speichern und Upload finalisieren.");
// 		saveUploadedFile(req.request_body, location->upload_store + "/");
// 		finalizeUpload(req);
// 	}
// }

// bool RequestHandler::handleChunkedUpload(RequestBody &req, const std::string &request, size_t headerEnd, const Location* loc)
// {
// 	size_t bodyStart = headerEnd + 4;
// 	std::string chunkData = request.substr(bodyStart);

// 	if (!req.chunked_state.processing)
// 	{
// 		req.chunked_state.processing = true;
// 	}
// 	req.chunked_state.buffer += chunkData;

// 	while (true)
// 	{
// 		size_t chunkHeaderEnd = req.chunked_state.buffer.find("\r\n");
// 		if (chunkHeaderEnd == std::string::npos)
// 			return false;

// 		std::string chunkSizeHex = req.chunked_state.buffer.substr(0, chunkHeaderEnd);
// 		size_t chunkSize = 0;
// 		{
// 			std::stringstream ss;
// 			ss << std::hex << chunkSizeHex;
// 			ss >> chunkSize;
// 		}

// 		if (req.chunked_state.buffer.size() < chunkHeaderEnd + 2 + chunkSize + 2)
// 			return false;

// 		if (chunkSize == 0)
// 		{
// 			req.chunked_state.processing = false;

// 			std::string uploadPath = loc->upload_store;
// 			if (!uploadPath.empty() && uploadPath.back() != '/')
// 				uploadPath.push_back('/');
// 			uploadPath += "chunked_upload.dat";

// 			std::ofstream ofs(uploadPath, std::ios::binary);
// 			ofs.write(req.received_body.c_str(), req.received_body.size());
// 			ofs.close();

// 			return true;
// 		}

// 		std::string chunk = req.chunked_state.buffer.substr(chunkHeaderEnd + 2, chunkSize);
// 		req.received_body += chunk;

// 		req.chunked_state.buffer.erase(0, chunkHeaderEnd + 2 + chunkSize + 2);

// 		if (req.received_body.size() > (size_t)loc->client_max_body_size)
// 		{
// 			return false;
// 		}
// 	}
// }

// void RequestHandler::saveUploadedFile(const std::string &body, const std::string &path) {
// 	//Logger::file("[Upload] Starting file upload process");

// 	// Extract filename
// 	std::regex filenameRegex(R"(Content-Disposition:.*filename=\"([^\"]+)\")");
// 	std::smatch matches;

// 	std::string filename;
// 	if (std::regex_search(body, matches, filenameRegex) && matches.size() > 1) {
// 		filename = matches[1].str();
// 		//Logger::file("[Upload] Extracted filename: " + filename);
// 	} else {
// 		//Logger::file("[Upload] ERROR: Failed to extract filename from body");
// 		return;
// 	}

// 	// Find content boundaries
// 	size_t contentStart = body.find("\r\n\r\n");
// 	if (contentStart == std::string::npos) {
// 		//Logger::file("[Upload] ERROR: Failed to find content start boundary");
// 		return;
// 	}
// 	contentStart += 4;
// 	//Logger::file("[Upload] Content starts at position: " + std::to_string(contentStart));

// 	size_t contentEnd = body.rfind("\r\n--");
// 	if (contentEnd == std::string::npos) {
// 		//Logger::file("[Upload] ERROR: Failed to find content end boundary");
// 		return;
// 	}
// 	//Logger::file("[Upload] Content ends at position: " + std::to_string(contentEnd));

// 	// Extract content
// 	std::string fileContent = body.substr(contentStart, contentEnd - contentStart);
// 	//Logger::file("[Upload] Extracted file content size: " + std::to_string(fileContent.size()));

// 	// Build full path and save
// 	std::string filePath = path + filename;
// 	//Logger::file("[Upload] Saving to path: " + filePath);

// 	std::ofstream outputFile(filePath, std::ios::binary);
// 	if (!outputFile) {
// 		//Logger::file("[Upload] ERROR: Failed to open output file: " + filePath);
// 		return;
// 	}

// 	outputFile.write(fileContent.c_str(), fileContent.size());
// 	outputFile.close();

// 	if (outputFile) {
// 		//Logger::file("[Upload] File successfully saved");

// 		std::string successResponse = "HTTP/1.1 200 OK\r\n"
// 									"Content-Type: text/plain\r\n"
// 									"Content-Length: 13\r\n"
// 									"\r\n"
// 									"Upload success";

// 		//Logger::file("[Upload] Success response prepared");
// 	} else {
// 		//Logger::file("[Upload] ERROR: Failed to write file");
// 	}
// }



// void RequestHandler::finalizeUpload(RequestBody &req) {
// 	//Logger::file("[Upload] Finalizing upload process");

// 	std::string successResponse = "HTTP/1.1 200 OK\r\n"
// 								"Content-Type: text/plain\r\n"
// 								"Content-Length: 13\r\n"
// 								"\r\n"
// 								"Upload success";

// 	req.response_buffer.clear();
// 	req.response_buffer.insert(req.response_buffer.end(),
// 							successResponse.begin(),
// 							successResponse.end());

// 	req.state = RequestBody::STATE_SENDING_RESPONSE;

// 	server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);

// 	//Logger::file("[Upload] Upload finalized, response ready to send");
// }

// bool detectMultipartFormData(RequestBody &req)
// {
// 	std::string bufferStr(req.request_buffer.begin(), req.request_buffer.end());

// 	if (bufferStr.find("Content-Type: multipart/form-data") != std::string::npos)
// 	{
// 		req.is_multipart = true;
// 		//Logger::file("[detectMultipartFormData] multipart/form-data erkannt.");
// 		return true;
// 	}

// 	req.is_multipart = false;
// 	//Logger::file("[detectMultipartFormData] Kein multipart/form-data.");
// 	return false;
// }

// void RequestHandler::parseRequest(RequestBody &req)
// {

// 	req.method = getMethod(req.request_buffer);
// 	//Logger::file("[parseRequest] Starte Parse. Phase = " + std::to_string(req.parsing_phase) + ", request_buffer size: " + std::to_string(req.request_buffer.size()));

// 	if (req.parsing_phase == RequestBody::PARSING_HEADER)
// 	{
// 		std::string bufferContent(req.request_buffer.begin(), req.request_buffer.end());
// 		size_t headerEndPos = bufferContent.find("\r\n\r\n");
// 		if (headerEndPos == std::string::npos)
// 		{
// 			//Logger::file("[parseRequest] Header unvollständig -> warte auf mehr Daten");
// 			return;
// 		}

// 		// Header-Teil herauslösen
// 		std::string headers = bufferContent.substr(0, headerEndPos);
// 		//Logger::file("[parseRequest] Header gefunden bis Pos: " + std::to_string(headerEndPos));
// 		//Logger::file("[parseRequest] Headers:\n" + headers);

// 		std::istringstream headerStream(headers);
// 		std::string requestLine;
// 		std::getline(headerStream, requestLine);

// 		if (requestLine.empty())
// 		{
// 			//Logger::file("[parseRequest] Request-Line leer -> warten...");
// 			return;
// 		}

// 		// Request-Line splitten in METHOD PATH VERSION
// 		std::string method, path, version;
// 		{
// 			std::istringstream rlstream(requestLine);
// 			rlstream >> method >> path >> version;
// 			if (path.empty()) path = "/";
// 		}

// 		//Logger::file("[parseRequest] Methode: '" + method + "', Pfad: '" + path + "', Version: '" + version + "'");

// 		static const std::set<std::string> validMethods = {
// 		    "GET", "POST", "DELETE", "HEAD", "PUT", "OPTIONS", "PATCH"
// 		};
// 		if (validMethods.find(method) == validMethods.end())
// 		{
// 			//Logger::file("[parseRequest] Unbekannte Methode: '" + method + "' -> warte");
// 			return;
// 		}

// 		// Query-Teil extrahieren falls ? vorhanden
// 		std::string query;
// 		size_t qpos = path.find('?');
// 		if (qpos != std::string::npos)
// 		{
// 			query = path.substr(qpos + 1);
// 			path  = path.substr(0, qpos);
// 		}

// 		// Header untersuchen auf Content-Length oder Transfer-Encoding
// 		bool   is_chunked      = false;
// 		size_t content_length  = 0;
// 		std::string line;
// 		while (std::getline(headerStream, line) && !line.empty())
// 		{
// 			if (!line.empty() && line.back() == '\r')
// 				line.pop_back();

// 			if (line.rfind("Cookie:", 0) == 0)
// 			{
// 				req.cookie_header = line.substr(std::strlen("Cookie: "));
// 			}
// 			else if (line.rfind("Content-Length:", 0) == 0)
// 			{
// 				content_length = static_cast<size_t>(
// 					std::stoul(line.substr(std::strlen("Content-Length: ")))
// 				);
// 				//Logger::file("[parseRequest] Content-Length: " + std::to_string(content_length));
// 			}
// 			else if (line.rfind("Transfer-Encoding:", 0) == 0)
// 			{
// 				std::string encoding = line.substr(std::strlen("Transfer-Encoding: "));
// 				if (encoding.find("chunked") != std::string::npos)
// 				{
// 					//Logger::file("[parseRequest] Transfer-Encoding: chunked erkannt.");
// 					is_chunked = true;
// 				}
// 			}
// 		}

// 		const Location* location = findMatchingLocation(req.associated_conf, path);
// 		if (!location)
// 		{
// 			//Logger::file("[parseRequest] Kein passendes Location -> 404 senden.");
// 			std::stringstream errorStream;
// 			buildErrorResponse(404, "Not Found", &errorStream, req);
// 			std::string err = errorStream.str();
// 			req.response_buffer.clear();
// 			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
// 			req.state = RequestBody::STATE_SENDING_RESPONSE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}

// 		// Body-Anteil, der ggf. in diesem Paket schon vorliegt
// 		size_t bodyPos = headerEndPos + 4;
// 		std::string bodyPart;
// 		if (bufferContent.size() > bodyPos)
// 			bodyPart = bufferContent.substr(bodyPos);

// 		// Header gelesen => wir löschen den Header-Puffer
// 		req.request_buffer.clear();

// 		// Grundlegende RequestBody-Felder belegen
// 		req.method         = method;
// 		req.location_path  = path;
// 		req.requested_path = buildRequestedPath(req, path);
// 		req.content_length = content_length;

// 		// multipart-FormData-Erkennung
// 		detectMultipartFormData(req);

// 		// Optional: Weiterer Code, z.B. Debug-Ausgabe
// 		printRequestBody(req);

// 		// Check auf Redirect
// 		std::stringstream redirectResponse;
// 		if (checkRedirect(req, &redirectResponse))
// 		{
// 			//Logger::file("[parseRequest] Redirect definiert, send Response");
// 			req.state = RequestBody::STATE_SENDING_RESPONSE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}

// 		// Behandlung der Methoden
// 		if (method == "POST")
// 		{
// 			if (is_chunked)
// 			{
// 				//Logger::file("[parseRequest] POST mit chunked Transfer-Encoding");
// 				if (!handleChunkedUpload(req, bufferContent, headerEndPos, location))
// 				{
// 					//Logger::file("[parseRequest] Noch nicht alle Chunks -> warten.");
// 					return;
// 				}
// 				// Wenn handleChunkedUpload() true zurückgibt, ist der Upload abgeschlossen.
// 				req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 				return;
// 			}

// 			// Normaler Content-Length basierter POST
// 			if (content_length > (size_t)location->client_max_body_size)
// 			{
// 				//Logger::file("[parseRequest] 413 Payload Too Large");
// 				std::stringstream errorStream;
// 				buildErrorResponse(413, "Payload Too Large", &errorStream, req);
// 				std::string err = errorStream.str();
// 				req.response_buffer.clear();
// 				req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
// 				req.state = RequestBody::STATE_SENDING_RESPONSE;
// 				server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 				return;
// 			}

// 			// bodyPart direkt in request_body übernehmen
// 			req.request_body = bodyPart;

// 			// Haben wir schon alles?
// 			if (req.request_body.size() >= content_length)
// 			{
// 				//Logger::file("[parseRequest] POST-Body vollständig im ersten Paket.");
// 				handlePostBodyComplete(req, location);
// 				req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 				return;
// 			}
// 			else
// 			{
// 				//Logger::file("[parseRequest] POST-Body noch unvollständig -> PARSING_BODY");
// 				req.parsing_phase = RequestBody::PARSING_BODY;
// 				return;
// 			}
// 		}
// 		else if (method == "DELETE")
// 		{
// 			//Logger::file("[parseRequest] DELETE-Anfrage erkannt");
// 			handleDeleteRequest(req);
// 			req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}
// 		else
// 		{
// 			//Logger::file("[parseRequest] " + method + "-Anfrage -> weiterverarbeiten.");
// 			if (server.getCgiHandler()->needsCGI(req, path))
// 			{
// 				//Logger::file("[parseRequest] -> CGI");
// 				req.state = RequestBody::STATE_PREPARE_CGI;
// 				server.getCgiHandler()->addCgiTunnel(req, method, query);
// 			}
// 			else
// 			{
// 				//Logger::file("[parseRequest] -> Statisches buildResponse()");
// 				buildResponse(req);
// 				req.state = RequestBody::STATE_SENDING_RESPONSE;
// 			}
// 			req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}
// 	}
// 	else if (req.parsing_phase == RequestBody::PARSING_BODY)
// 	{
// 		//Logger::file("[parseRequest] PARSING_BODY -> empfange Restdaten.");

// 		// Alles, was an neuem Datenmaterial eingetroffen ist, ansammeln
// 		if (!req.request_buffer.empty())
// 		{
// 			req.request_body.append(req.request_buffer.begin(), req.request_buffer.end());
// 			req.request_buffer.clear();
// 		}

// 		const Location* location = findMatchingLocation(req.associated_conf, req.location_path);
// 		if (!location)
// 		{
// 			//Logger::file("[parseRequest] Location weg -> 404");
// 			std::stringstream errorStream;
// 			buildErrorResponse(404, "Not Found", &errorStream, req);
// 			std::string err = errorStream.str();
// 			req.response_buffer.clear();
// 			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
// 			req.state = RequestBody::STATE_SENDING_RESPONSE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}

// 		// Maximalgröße prüfen
// 		if (req.request_body.size() > (size_t)location->client_max_body_size)
// 		{
// 			//Logger::file("[parseRequest] 413 Payload Too Large (PARSING_BODY)");
// 			std::stringstream errorStream;
// 			buildErrorResponse(413, "Payload Too Large", &errorStream, req);
// 			std::string err = errorStream.str();
// 			req.response_buffer.clear();
// 			req.response_buffer.insert(req.response_buffer.end(), err.begin(), err.end());
// 			req.state = RequestBody::STATE_SENDING_RESPONSE;
// 			server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
// 			return;
// 		}

// 		// Wenn vollständig -> handlePostBodyComplete
// 		if (req.request_body.size() >= req.content_length)
// 		{
// 			//Logger::file("[parseRequest] Body nun komplett -> handlePostBodyComplete.");
// 			handlePostBodyComplete(req, location);
// 			req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 			return;
// 		}
// 		else
// 		{
// 			//Logger::file("[parseRequest] Body noch nicht komplett -> weiter warten.");
// 			return;
// 		}
// 	}
// 	else if (req.parsing_phase == RequestBody::PARSING_COMPLETE)
// 	{
// 		//Logger::file("[parseRequest] PARSING_COMPLETE -> Keine weitere Aktion.");
// 		return;
// 	}
// }