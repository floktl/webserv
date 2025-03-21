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
			ctx.multipart_file_path_up_down = concatenatePath(ctx.location.upload_store, filename);
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
	if (ctx.multipart_fd_up_down >= 0) {
		return;
	}

	struct stat prev_stat;
	if (stat(ctx.multipart_file_path_up_down.c_str(), &prev_stat) == 0) {
		if (S_ISDIR(prev_stat.st_mode)) {
			Logger::errorLog("Upload file is a directory: " + ctx.multipart_file_path_up_down);
			updateErrorStatus(ctx, 400, "Bad Request");
			return;
		}
	}

	std::string upload_dir = ctx.multipart_file_path_up_down.substr(0, ctx.multipart_file_path_up_down.find_last_of("/"));

	struct stat dir_stat;
	if (stat(upload_dir.c_str(), &dir_stat) < 0) {
		Logger::errorLog("Upload directory does not exist: " + upload_dir);
		updateErrorStatus(ctx, 404, "Upload directory not found");
		return;
	}

	if (!S_ISDIR(dir_stat.st_mode)) {
		Logger::errorLog("Upload path is not a directory: " + upload_dir);
		updateErrorStatus(ctx, 400, "Bad Request");
		return;
	}

	if (access(upload_dir.c_str(), W_OK) < 0) {
		Logger::errorLog("Upload directory not writable: " + upload_dir);
		updateErrorStatus(ctx, 403, "Upload directory not writable");
		return;
	}
	// Check IF File Already Exists
	if (access(ctx.multipart_file_path_up_down.c_str(), F_OK) == 0) {
		Logger::errorLog("File already exists: " + ctx.multipart_file_path_up_down);
		updateErrorStatus(ctx, 409, "File already exists");
		return;
	}

	ctx.multipart_fd_up_down = open(ctx.multipart_file_path_up_down.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (ctx.multipart_fd_up_down < 0) {
		updateErrorStatus(ctx, 500, "Failed to create upload file");
		return;
	}

}

// Parses and Sets Access Rights for a Request Based on Server Configuration
void Server::parseAccessRights(Context& ctx)
{
	std::string req_root = retreiveReqRoot(ctx);
	if (ctx.method == "DELETE")
	{
		ctx.path = "/" + ctx.location.upload_store + ctx.path;
		ctx.requested_path = ctx.path;
		if (ctx.type == CGI)
		{
			ctx.wasCgiDel = true;
			ctx.type = STATIC;
		}
	}
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
		if (!requestedPath.empty() && requestedPath.back() == '/' && ctx.location.autoindex == "on")
		{
			std::string tmppath = concatenatePath(requestedPath, ctx.location.default_file);
			if (fileReadable(tmppath))
			{
				requestedPath = tmppath;
			}
			else
			{
				ctx.doAutoIndex = requestedPath;
			}
		}
		else if (!requestedPath.empty() && requestedPath.back() == '/')
			requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
		if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
			return;
		requestedPath = approveExtention(ctx, requestedPath);
	}

	if (ctx.type == ERROR)
		return;
	if (ctx.type != REDIRECT && !ctx.wasCgiDel)
		checkAccessRights(ctx, requestedPath);
	if (ctx.type == ERROR)
		return;
	ctx.approved_req_path = requestedPath;
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
