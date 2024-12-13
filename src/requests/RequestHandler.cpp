/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 13:51:57 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#include "RequestHandler.hpp"
#include "./../utils/Logger.hpp"

#include <sys/epoll.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/select.h>
#include <cstddef>

// Konstruktor
RequestHandler::RequestHandler(
	int _client_fd,
	const ServerBlock& _config,
	std::set<int>& _activeFds,
	std::map<int,const ServerBlock*>& _serverBlockConfigs
) : client_fd(_client_fd),
	config(_config),
	activeFds(_activeFds),
	serverBlockConfigs(_serverBlockConfigs),
	errorHandler(_client_fd, _config.errorPages),
	state(STATE_READING_REQUEST),
	requestComplete(false),
	cgiNeeded(false),
	cgiStarted(false),
	cgiFinished(false),
	responseSent(false)
{
	// Debug: Konstruktor wird aufgerufen, Client-FD: client_fd, State = STATE_READING_REQUEST
	Logger::green("RequestHandler: Konstruktor aufgerufen. Client-FD: " + std::to_string(client_fd));
}

bool RequestHandler::parseRequestLine(const std::string& request, std::string& method,
							std::string& requestedPath, std::string& version)
{
	// Debug: Parsing der ersten Request-Zeile (Request-Line)
	std::istringstream requestStream(request);
	std::string requestLine;
	if (!std::getline(requestStream, requestLine))
		return false;
	if (!requestLine.empty() && requestLine.back() == '\r')
		requestLine.pop_back();

	// Debug: Zerlegen der Request-Line in Methode, Pfad und Version
	std::istringstream lineStream(requestLine);
	lineStream >> method >> requestedPath >> version;
	if (method.empty() || requestedPath.empty() || version.empty())
		return false;

	// Debug: Request-Line erfolgreich geparst
	Logger::blue("Request line parsed: Method: " + method + " | Path: " + requestedPath + " | Version: " + version);
	return true;
}

void RequestHandler::parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headersMap)
{
	// Debug: Beginn des Header-Parsings
	std::string headerLine;
	while (std::getline(requestStream, headerLine) && headerLine != "\r") {
		if (!headerLine.empty() && headerLine.back() == '\r')
			headerLine.pop_back();
		size_t colonPos = headerLine.find(":");
		if (colonPos != std::string::npos) {
			std::string key = headerLine.substr(0, colonPos);
			std::string value = headerLine.substr(colonPos + 1);
			while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
				value.erase(value.begin());
			headersMap[key] = value;
			// Debug: Einzelner Header geparst
			Logger::yellow("Header parsed: " + key + " = " + value);
		}
	}
}

std::string RequestHandler::extractRequestBody(std::istringstream& requestStream)
{
	// Debug: Extrahieren des Request-Bodys (bei POST)
	std::string requestBody;
	std::string remaining;
	while (std::getline(requestStream, remaining))
		requestBody += remaining + "\n";
	if (!requestBody.empty() && requestBody.back() == '\n')
		requestBody.pop_back();

	// Debug: Request-Body extrahiert
	Logger::blue("Request body length: " + std::to_string(requestBody.size()));
	return requestBody;
}

const Location* RequestHandler::findLocation(const std::string& requestedPath)
{
	// Debug: Suche nach passender Location für angeforderten Pfad
	for (const auto& loc : config.locations) {
		// Debug: Prüfe Location-Prefix: " + loc.path
		if (requestedPath.find(loc.path) == 0) {
			Logger::green("Matching location found: " + loc.path);
			return &loc;
		}
	}
	// Debug: Keine passende Location gefunden
	Logger::red("No location match found for: " + requestedPath);
	return nullptr;
}

bool RequestHandler::handleDeleteMethod(const std::string& filePath)
{
	// Debug: DELETE-Methoden-Handling für Datei: filePath
	if (remove(filePath.c_str()) == 0) {
		Logger::green("File deleted successfully: " + filePath);
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
							"<html><body><h1>File Deleted</h1></body></html>";
		responseBuffer = response;
		state = STATE_SENDING_RESPONSE;
		return true;
	} else {
		Logger::red("Delete failed for file: " + filePath);
		errorHandler.sendErrorResponse(404, "File Not Found");
		closeConnection();
		return true;
	}
}

bool RequestHandler::handleDirectoryIndex(std::string& filePath)
{
	// Debug: Prüfen ob Pfad ein Verzeichnis ist und ggf. Indexdatei suchen
	DIR* dir = opendir(filePath.c_str());
	if (dir != nullptr) {
		closedir(dir);

		// Debug: Verzeichnis bestätigt, suche nach Indexdatei
		std::string indexFiles = config.index;
		if (indexFiles.empty()) {
			Logger::red("No index defined for directory: " + filePath);
			errorHandler.sendErrorResponse(403, "Forbidden");
			closeConnection();
			return true;
		}

		std::istringstream iss(indexFiles);
		std::string singleIndex;
		bool foundIndex = false;
		while (iss >> singleIndex) {
			std::string tryPath = filePath;
			if (tryPath.back() != '/')
				tryPath += "/";
			tryPath += singleIndex;

			std::ifstream testFile(tryPath);
			if (testFile.is_open()) {
				filePath = tryPath;
				testFile.close();
				foundIndex = true;
				Logger::green("Index file found: " + filePath);
				break;
			}
		}

		if (!foundIndex) {
			Logger::red("No index file found in directory: " + filePath);
			errorHandler.sendErrorResponse(404, "File Not Found");
			closeConnection();
			return true;
		}
	}
	return false;
}

void RequestHandler::handleStaticFile(const std::string& filePath)
{
	// Debug: Versuche statische Datei zu öffnen: filePath
	std::ifstream file(filePath);
	if (file.is_open()) {
		std::stringstream fileContent;
		fileContent << file.rdbuf();
		std::string body = fileContent.str(); // Hier wandeln wir den Inhalt in einen std::string um

		std::string response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: " + std::to_string(body.size()) + "\r\n"
			"Connection: close\r\n"
			"\r\n" +
			body;

		responseBuffer = response;
		state = STATE_SENDING_RESPONSE;
		Logger::green("Static file served successfully: " + filePath);
	} else {
		Logger::red("Failed to open file: " + filePath);
		errorHandler.sendErrorResponse(404, "File Not Found");
		closeConnection();
	}
}


void RequestHandler::handleStateReadingRequest()
{
	// Debug: Lese Request-Daten vom Client-Socket
	char buf[4096];
	for (;;) {
		ssize_t n = recv(client_fd, buf, sizeof(buf), 0);
		if (n > 0) {
			// Debug: Daten empfangen, n Bytes gelesen
			requestBuffer.append(buf, n);
			if (requestBuffer.find("\r\n\r\n") != std::string::npos) {
				// Debug: Request-Header vollständig empfangen
				requestComplete = true;
				Logger::green("Complete request header received. Parsing now...");
				parseFullRequest();
				return;
			}
		} else if (n == 0) {
			// Debug: Client hat Verbindung geschlossen
			Logger::yellow("Client closed the connection");
			closeConnection();
			return;
		} else {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// Debug: Keine weiteren Daten verfügbar, später erneut versuchen
				return;
			} else {
				// Debug: Lese-Fehler
				Logger::red("Error reading request: " + std::string(strerror(errno)));
				closeConnection();
				return;
			}
		}
	}
}

void RequestHandler::parseFullRequest()
{
	// Debug: Starte vollständiges Parsing des Requests
	std::string method, requestedPath, version;

	if (!parseRequestLine(requestBuffer, method, requestedPath, version)) {
		Logger::red("Failed to parse request line");
		errorHandler.sendErrorResponse(400, "Bad Request");
		closeConnection();
		return;
	}

	// Debug: Request-Line erfolgreich, nun Headers parsen
	std::istringstream requestStream(requestBuffer);
	std::string dummyLine;
	std::getline(requestStream, dummyLine);
	std::map<std::string, std::string> headersMap;
	parseHeaders(requestStream, headersMap);

	std::string requestBody;
	if (method == "POST") {
		// Debug: POST-Request, Body wird gelesen
		requestBody = extractRequestBody(requestStream);
	}

	const Location* location = findLocation(requestedPath);
	if (!location) {
		Logger::red("No matching location for request path: " + requestedPath);
		errorHandler.sendErrorResponse(404, "Not Found");
		closeConnection();
		return;
	}

	// Debug: Location gefunden, verwende Root aus Location oder ServerBlock
	std::string root = location->root.empty() ? config.root : location->root;
	std::string filePath = root + requestedPath;

	// Debug: Überprüfen ob DELETE-Request
	if (method == "DELETE") {
		if (handleDeleteMethod(filePath)) return;
	}

	// Debug: Überprüfen ob Verzeichnis und Indexdatei verwendet werden muss
	if (handleDirectoryIndex(filePath)) {
		return;
	}

	// Debug: Prüfe ob CGI ausgeführt werden soll
	if (!location->cgi.empty()) {
		cgiHandler.reset(new CgiHandler(
			client_fd, location, filePath, method, requestedPath, requestBody,
			headersMap, activeFds, config, serverBlockConfigs
		));

		if (cgiHandler->handleCGIIfNeeded()) {
			// Debug: CGI wird asynchron behandelt. State wird auf STATE_CGI_RUNNING gesetzt.
			state = STATE_CGI_RUNNING;
			return;
		} else {
			// Debug: CGI konnte nicht gestartet werden
			Logger::red("Failed to start CGI process");
			errorHandler.sendErrorResponse(500, "Internal Server Error");
			closeConnection();
			return;
		}
	}

	// Debug: Keine CGI nötig, handle statische Datei bei GET oder POST
	if (method == "GET" || method == "POST") {
		handleStaticFile(filePath);
		return;
	}

	// Debug: Methode nicht erlaubt
	errorHandler.sendErrorResponse(405, "Method Not Allowed");
	closeConnection();
}

void RequestHandler::handle_request()
{
	// Debug: handle_request aufgerufen, aktueller State: state
	std::cout << "handle_request" << std::endl;
	switch (state) {
		case STATE_READING_REQUEST:
			std::cout << "STATE_READING_REQUEST" << std::endl;
			Logger::blue("State: Reading Request from client_fd: " + std::to_string(client_fd));
			handleStateReadingRequest();
			break;
		case STATE_CGI_RUNNING:
			std::cout << "STATE_CGI_RUNNING" << std::endl;
			Logger::blue("State: CGI Running");
			break;
		case STATE_SENDING_RESPONSE:
			std::cout << "STATE_SENDING_RESPONSE" << std::endl;
			Logger::blue("State: Sending Response");
			handleStateSendingResponse();
			break;
		case STATE_DONE:
			std::cout << "STATE_DONE" << std::endl;
			Logger::blue("State: Done - Closing connection");
			closeConnection();
			break;
		default:
			Logger::red("Unknown state encountered");
			break;
	}
}

void RequestHandler::handleStateSendingResponse()
{
	// Debug: Sende die Response an den Client, solange noch Daten im responseBuffer sind
	if (responseSent)
		return;

	for (;;) {
		ssize_t n = send(client_fd, responseBuffer.data(), responseBuffer.size(), 0);
		if (n > 0) {
			// Debug: n Bytes gesendet
			responseBuffer.erase(0, n);
			if (responseBuffer.empty()) {
				// Debug: Alle Daten gesendet
				responseSent = true;
				Logger::green("Response fully sent to client_fd: " + std::to_string(client_fd));
				closeConnection();
				return;
			}
		} else {
			if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
				// Debug: Socket momentan blockiert, später erneut senden
				return;
			} else {
				// Debug: Sende-Fehler, Verbindung schließen
				Logger::red("Error sending response: " + std::string(strerror(errno)));
				closeConnection();
				return;
			}
		}
	}
}

void RequestHandler::closeConnection()
{
	// Debug: Schließe Verbindung, entferne client_fd aus activeFds und serverBlockConfigs
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
	state = STATE_DONE;
	Logger::yellow("Connection closed for client_fd: " + std::to_string(client_fd));
}