#include "Server.hpp"


// Extracts and Validates http Headers from the Request, Updating the Request Context
bool Server::parseBareHeaders(Context& ctx, std::vector<ServerBlock>& configs)
{
	size_t header_end = ctx.read_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return true;

	std::string headers = ctx.read_buffer.substr(0, header_end);
	std::istringstream stream(headers);

	if (!parseRequestLine(ctx, stream))
		return false;
	if (!parseHeaderFields(ctx, stream))
		return false;

	auto content_type_it = ctx.headers.find("Content-Type");
	if (content_type_it != ctx.headers.end() &&
		content_type_it->second.find("multipart/form-data") != std::string::npos) {
		parseMultipartHeaders(ctx);
	}

	std::string requested_host = extractHostname(ctx.read_buffer);
	int requested_port = extractPort(ctx.read_buffer);
	bool server_match_found = false;

	for (const auto& config : configs) {
		if (((config.name == requested_host && config.port == requested_port) ||
			(config.name == "localhost" && (requested_host == "localhost" || requested_host == "127.0.0.1")) ||
			(requested_host.empty() && config.port == requested_port)))
		{
			server_match_found = true;
			break;
		}
	}

	if (!server_match_found) {
		Logger::errorLog("No matching server block found - Closing connection");
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
	ctx.read_buffer.erase(0, header_end + 4);
	ctx.headers_complete = true;
	ctx.is_multipart = isMultipart(ctx);
	if (!ctx.is_multipart)
	{
		ctx.ready_for_ping_pong = 1;
	}
	if (ctx.method == "POST" && ctx.headers.find("Content-Length") == ctx.headers.end() &&
		ctx.headers.find("Transfer-Encoding") == ctx.headers.end())
		return updateErrorStatus(ctx, 411, "Length Required");

	if (!checkMaxContentLength(ctx, configs))
		return false;
	Logger::magenta("reach here");
	return true;
}

// New Function to Handle Multipart Form Data Headers
void Server::parseMultipartHeaders(Context& ctx) {
	std::string boundary;
	auto content_type_it = ctx.headers.find("Content-Type");
	auto it = ctx.headers.find("Content-Length");
	if (it != ctx.headers.end()) {
		try {
			ctx.req.expected_body_length = std::stoull(it->second);
		} catch (const std::invalid_argument& e) {
			ctx.req.expected_body_length = 0;
			std::cerr << "Invalid Content-Length value" << std::endl;
		}
	}
	if (content_type_it != ctx.headers.end()) {
		size_t boundary_pos = content_type_it->second.find("boundary=");
		if (boundary_pos != std::string::npos) {
			boundary = content_type_it->second.substr(boundary_pos + 9);
			ctx.headers["Content-Boundary"] = boundary;
		}
	}

	size_t boundary_start = ctx.read_buffer.find("--" + boundary);
	if (boundary_start == std::string::npos) {
		return;
	}

	size_t disp_pos = ctx.read_buffer.find("Content-Disposition: form-data", boundary_start);
	if (disp_pos == std::string::npos) {
		return;
	}

	size_t filename_pos = ctx.read_buffer.find("filename=\"", disp_pos);
	if (filename_pos != std::string::npos) {
		filename_pos += 10;
		size_t filename_end = ctx.read_buffer.find("\"", filename_pos);
		if (filename_end != std::string::npos) {
			std::string filename = ctx.read_buffer.substr(filename_pos, filename_end - filename_pos);
			ctx.headers["Content-Disposition-Filename"] = filename;
			ctx.uploaded_file_path = concatenatePath(ctx.location.upload_store, filename);
		}
	}

	size_t headers_end = ctx.read_buffer.find("\r\n\r\n", disp_pos);
	if (headers_end != std::string::npos) {
		ctx.header_offset = headers_end + 4;
	} else {
		ctx.header_offset = 0;
	}
}

// Parses and stores individual header Fields in the Request Context
bool Server::parseHeaderFields(Context& ctx, std::istringstream& stream) {
	std::string line;
	while (std::getline(stream, line)) {
		if (line.empty() || line == "\r")
			break;
		if (line.back() == '\r')
			line.pop_back();

		size_t colon = line.find(':');
		if (colon != std::string::npos) {
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			value = value.substr(value.find_first_not_of(" "));
			ctx.headers[key] = value;

			if (key == "Content-Length") {
				try {
					ctx.content_length = std::stoull(value);
				} catch (const std::exception& e) {
					updateErrorStatus(ctx, 400, "Bad Request - Invalid Content-Length");
					return false;
				}
			}
			else if (key == "Cookie") {
				parseCookies(ctx, value);
			}
		}
	}
	return true;
}

// Parses the Request Line (Method, Path, version) from the input Buffer
bool Server::parseRequestLine(Context& ctx, std::istringstream& stream)
{
	std::string line;

	if (!std::getline(stream, line))
	{
		updateErrorStatus(ctx, 400, "Bad Request - Empty Request");
		return false;
	}

	if (line.back() == '\r')
		line.pop_back();

	std::istringstream request_line(line);
	request_line >> ctx.method >> ctx.path >> ctx.version;

	if (ctx.method.empty() || ctx.path.empty() || ctx.version.empty())
	{
		updateErrorStatus(ctx, 400, "Bad Request - Invalid Request Line");
		return false;
	}

	return true;
}

void Server::prepareUploadPingPong(Context& ctx)
{
	std::string upload_dir = ctx.uploaded_file_path.substr(0, ctx.uploaded_file_path.find_last_of("/"));

	struct stat dir_stat;
	if (stat(upload_dir.c_str(), &dir_stat) < 0) {
		Logger::errorLog("Upload directory does not exist: " + upload_dir);
		updateErrorStatus(ctx, 404, "Upload directory not found");
		return;
	}

	if (!S_ISDIR(dir_stat.st_mode)) {
		Logger::errorLog("Upload path is not a directory: " + upload_dir);
		updateErrorStatus(ctx, 400, "Invalid upload directory");
		return;
	}

	if (access(upload_dir.c_str(), W_OK) < 0) {
		Logger::errorLog("Upload directory not writable: " + upload_dir);
		updateErrorStatus(ctx, 403, "Upload directory not writable");
		return;
	}

// Check IF File Already Exists
	if (access(ctx.uploaded_file_path.c_str(), F_OK) == 0) {
		Logger::errorLog("File already exists: " + ctx.uploaded_file_path);
		updateErrorStatus(ctx, 409, "File already exists");
		return;
	}

	ctx.upload_fd = open(ctx.uploaded_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (ctx.upload_fd < 0) {
		updateErrorStatus(ctx, 500, "Failed to create upload file");
		return;
	}

}

// Parses and Sets Access Rights for a Request Based on Server Configuration
void Server::parseAccessRights(Context& ctx)
{
	std::string req_root = retreiveReqRoot(ctx);
	std::string requestedPath = concatenatePath(req_root, ctx.path);
	if (ctx.index.empty() && ctx.method != "DELETE")
		ctx.index = DEFAULT_FILE;
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;

	std::string adjustedPath = ctx.path;
	if (ctx.method != "DELETE")
	{
		if (!ctx.useLocRoot) {
			adjustedPath = subtractLocationPath(ctx.path, ctx.location);
		}
		requestedPath = concatenatePath(req_root, adjustedPath);
		if (!requestedPath.empty() && requestedPath.back() == '/')
			requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
		if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
			return;
		requestedPath = approveExtention(ctx, requestedPath);
	}

	if (ctx.type == ERROR)
		return;
	if (ctx.type != REDIRECT)
		checkAccessRights(ctx, requestedPath);
	if (ctx.type == ERROR)
		return;
	ctx.approved_req_path = requestedPath;
	Logger::blue(ctx.approved_req_path);
}

// Extracts The Port Number from the Http Host Header, Defaulting to 80 IF Missing
int extractPort(const std::string& header)
{
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos)
	{
		size_t port_pos = header.find(":", host_pos + 6);
		if (port_pos != std::string::npos)
		{
			size_t end_pos = header.find("\r\n", port_pos);
			if (end_pos != std::string::npos)
			{
				std::string port_str = header.substr(port_pos + 1, end_pos - port_pos - 1);
				port_str = trim(port_str);
				try
				{
					int port = std::stoi(port_str);
					if (port <= 0 || port > 65535)
					{
						Logger::red("Invalid port number: '" + port_str + "'");
						return -1;
					}
					return port;
				}
				catch (const std::exception& e)
				{
					return -1;
				}
			}
		}
		else
			return 80;
	}
	return -1;
}

// Extracts the hostname from the http host header, return on emty string if missing
std::string extractHostname(const std::string& header)
{
	std::string hostname;
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos)
	{
		size_t start = host_pos + 6;
		size_t end_pos = header.find("\r\n", start);
		if (end_pos != std::string::npos)
		{
			size_t colon_pos = header.find(":", start);
			if (colon_pos != std::string::npos && colon_pos < end_pos)
				hostname = header.substr(start, colon_pos - start);
			else
				hostname = header.substr(start, end_pos - start);
		}
	}
	return hostname;
}

// Processes Chunked Transfer Encoding, Extracting and Assembling Request Body Chunks
void Server::parseChunkedBody(Context& ctx)
{
	while (!ctx.read_buffer.empty())
	{
		if (!ctx.req.chunked_state.processing)
		{
			size_t pos = ctx.read_buffer.find("\r\n");
			if (pos == std::string::npos)
				return;

			std::string chunk_size_str = ctx.read_buffer.substr(0, pos);
			size_t chunk_size;
			std::stringstream ss;
			ss << std::hex << chunk_size_str;
			ss >> chunk_size;

			if (chunk_size == 0)
			{
				ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				ctx.req.is_upload_complete = true;
				ctx.read_buffer.clear();
				return;
			}
			ctx.read_buffer.erase(0, pos + 2);
			ctx.req.chunked_state.buffer = "";
			ctx.req.chunked_state.processing = true;
			ctx.req.expected_body_length += chunk_size;
		}

		size_t remaining = ctx.req.expected_body_length - ctx.req.current_body_length;
		size_t available = std::min(remaining, ctx.read_buffer.size());

		if (available > 0) {
			ctx.req.received_body += ctx.read_buffer.substr(0, available);
			ctx.req.current_body_length += available;
			ctx.read_buffer.erase(0, available);
		}
		if (ctx.req.current_body_length == ctx.req.expected_body_length)
		{
			if (ctx.read_buffer.size() >= 2 && ctx.read_buffer.substr(0, 2) == "\r\n")
			{
				ctx.read_buffer.erase(0, 2);
				ctx.req.chunked_state.processing = false;
			}
			else
				return;
		}
	}
}

// Validates Content-Lengh Header and Checks Against Server-Configureded Limits
bool Server::checkMaxContentLength(Context& ctx, std::vector<ServerBlock>& configs)
{
	auto headersit = ctx.headers.find("Content-Length");
	long long content_length;

	if (headersit != ctx.headers.end())
	{
		content_length = std::stoull(headersit->second);
		getMaxBodySizeFromConfig(ctx, configs);
		if (content_length > ctx.client_max_body_size)
			return updateErrorStatus(ctx, 413, "Payload too large");
	}
	return true;
}

// Finalizes Request Parsing by Determining the Request Type and Updating Epoll Events
bool Server::finalizeRequest(Context& ctx)
{
	if (isMultipart(ctx) &&
		ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
		!ctx.req.is_upload_complete) {

		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
		return true;
	}
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE)
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	else if (ctx.error_code != 0)
		return false;
	return true;
}
