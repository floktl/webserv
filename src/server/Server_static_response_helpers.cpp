#include "Server.hpp"

std::string determineContentType(const std::string& path) {
	std::string contentType = "text/plain";
	size_t dot_pos = path.find_last_of('.');
	if (dot_pos != std::string::npos) {
		std::string ext = path.substr(dot_pos + 1);
		if (ext == "html" || ext == "htm") contentType = "text/html";
		else if (ext == "css") contentType = "text/css";
		else if (ext == "js") contentType = "application/javascript";
		else if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
		else if (ext == "png") contentType = "image/png";
		else if (ext == "gif") contentType = "image/gif";
		else if (ext == "pdf") contentType = "application/pdf";
		else if (ext == "json") contentType = "application/json";
	}
	return contentType;
}

