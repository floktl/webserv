#include "./server.hpp"

size_t Server::findBodyStart(const std::string& buffer, Context& ctx) {
	const std::string boundaryMarker = "\r\n\r\n";
	size_t pos = buffer.find(boundaryMarker);

	if (pos != std::string::npos) {
		if (isMultipart(ctx)) {
			std::string contentType = ctx.headers["Content-Type"];
			size_t boundaryPos = contentType.find("boundary=");
			if (boundaryPos != std::string::npos) {
				size_t boundaryStart = boundaryPos + 9;
				size_t boundaryEnd = contentType.find(';', boundaryStart);
				if (boundaryEnd == std::string::npos) {
					boundaryEnd = contentType.length();
				}

				std::string boundary = "--" + contentType.substr(boundaryStart, boundaryEnd - boundaryStart);
				ctx.boundary = boundary;
				size_t boundaryInBuffer = buffer.find(boundary);
				if (boundaryInBuffer != std::string::npos) {
					return boundaryInBuffer + boundary.length() + 2;
				}
			}
		}
		return pos + boundaryMarker.length();
	}

	return 0;
}

std::string Server::extractBodyContent(const char* buffer, ssize_t bytes, Context& ctx) {
	std::string cleanBody;

	if (ctx.headers_complete) {
		std::string temp(buffer, bytes);
		size_t bodyStart = findBodyStart(temp, ctx);

		if (bodyStart < static_cast<size_t>(bytes)) {
			cleanBody.assign(buffer + bodyStart, buffer + bytes);

			const std::string boundaryMarker = "\r\n\r\n";
			size_t boundaryPos = cleanBody.find(boundaryMarker);
			if (boundaryPos != std::string::npos) {
				cleanBody = cleanBody.substr(boundaryPos + boundaryMarker.length());
			}

			size_t contentDispositionPos = cleanBody.find("Content-Disposition: form-data;");
			if (contentDispositionPos != std::string::npos) {
				size_t endPos = cleanBody.find("\r\n", contentDispositionPos);
				if (endPos != std::string::npos) {
					cleanBody.erase(contentDispositionPos, endPos - contentDispositionPos + 2);
				}
			}

			size_t contentTypePos = cleanBody.find("Content-Type:");
			if (contentTypePos != std::string::npos) {
				size_t endPos = cleanBody.find("\r\n", contentTypePos);
				if (endPos != std::string::npos) {
					cleanBody.erase(contentTypePos, endPos - contentTypePos + 2);
				}
			}

			size_t boundaryEndPos = cleanBody.rfind("--" + ctx.headers["Content-Type"].substr(ctx.headers["Content-Type"].find("boundary=") + 9));
			if (boundaryEndPos != std::string::npos) {
				cleanBody.erase(boundaryEndPos);
			}
		}
	}

	return cleanBody;
}


bool Server::isMultipart(Context& ctx) {
	bool isMultipartUpload = ctx.method == "POST" &&
		ctx.headers.find("Content-Type") != ctx.headers.end() &&
		ctx.headers["Content-Type"].find("multipart/form-data") != std::string::npos;

	return isMultipartUpload;
}


bool Server::readingTheBody(Context& ctx, const char* buffer, ssize_t bytes) {
	ctx.had_seq_parse = true;

	Logger::yellow("readingTheBody begin");
	if (ctx.boundary.empty()) {
		Logger::yellow("ctx.boundary.empty()");
		auto content_type_it = ctx.headers.find("Content-Type");
		if (content_type_it != ctx.headers.end()) {
			Logger::yellow("content_type_it != ctx.headers.end()");
			size_t boundary_pos = content_type_it->second.find("boundary=");
			if (boundary_pos != std::string::npos) {
				Logger::yellow("boundary_pos != std::string::npos");
				ctx.boundary = "--" + content_type_it->second.substr(boundary_pos + 9);
				size_t end_pos = ctx.boundary.find(';');
				if (end_pos != std::string::npos) {
					Logger::yellow("end_pos != std::string::npos");
					ctx.boundary = ctx.boundary.substr(0, end_pos);
				}
			}
		}

		if (ctx.boundary.empty()) {
			Logger::errorLog("Could not determine boundary from headers");
			return false;
		}
	}

	ctx.write_buffer.clear();
	bool extraction_result = extractFileContent(ctx.boundary, std::string(buffer, bytes), ctx.write_buffer, ctx);

	if (ctx.upload_fd > 0 && !ctx.write_buffer.empty()) {
		ctx.write_pos = 0;
		ctx.write_len = ctx.write_buffer.size();
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	} else if (ctx.upload_fd > 0) {
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
	}

	return extraction_result;
}