#include "Server.hpp"

// Logs detailed context information, including FDs, request type, headers, and activity timestamp
void Server::logContext(const Context& ctx, const std::string& event)
{
	std::string log = "Context [" + event + "]:\n";

	log += "FDs: epoll=" + (ctx.epoll_fd != -1 ? std::to_string(ctx.epoll_fd) : "[empty]");
	log += " server=" + (ctx.server_fd != -1 ? std::to_string(ctx.server_fd) : "[empty]");
	log += " client=" + (ctx.client_fd != -1 ? std::to_string(ctx.client_fd) : "[empty]") + "\n";

	log += "Type: " + requestTypeToString(ctx.type) + "\n";

	log += "Method: " + (!ctx.method.empty() ? ctx.method : "[empty]") + "\n";
	log += "Path: " + (!ctx.path.empty() ? ctx.path : "[empty]") + "\n";
	log += "Version: " + (!ctx.version.empty() ? ctx.version : "[empty]") + "\n";

	log += "Headers:\n";
	if (ctx.headers.empty())
	{
		log += "[empty]\n";
	}
	else
	{
		for (const auto& header : ctx.headers)
		{
			log += header.first + ": " + header.second + "\n";
		}
	}

	log += "Body: " + (!ctx.body.empty() ? ctx.body : "[empty]") + "\n";

	log += "Location Path: " + (!ctx.location_path.empty() ? ctx.location_path : "[empty]") + "\n";
	log += "Requested Path: " + (!ctx.requested_path.empty() ? ctx.requested_path : "[empty]") + "\n";

	log += std::string("Location: ") + (ctx.location_inited ? "Defined" : "[empty]") + "\n";


	log += "Last Activity: " +
		std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
							ctx.last_activity.time_since_epoch())
							.count()) +
		" seconds since epoch";

	Logger::file(log);
}

// Logs active file descriptors in the request body map
void Server::logRequestBodyMapFDs()
{
	if (globalFDS.context_map.empty())
		return;

	std::string log = "Active FDs: ";
	for (const auto& pair : globalFDS.context_map)
		log += std::to_string(pair.first) + " ";
}

// Logs server configuration details, including ports, root paths, timeouts, and error pages
void log_server_configs(const std::vector<ServerBlock>& configs)
{
    Logger::file("Server Configurations:");
    Logger::file("[");

    if (configs.empty())
        Logger::file("   empty");

    for (const auto& server : configs)
	{
        Logger::file("   ServerBlock {");
        Logger::file("      port: " + std::to_string(server.port));
        Logger::file("      server_fd: " + std::to_string(server.server_fd));
        Logger::file("      name: " + server.name);
        Logger::file("      root: " + server.root);
        Logger::file("      index: " + server.index);
        Logger::file("      timeout: " + std::to_string(server.timeout));
        Logger::file("      client_max_body_size: " + std::to_string(server.client_max_body_size));

        // Log error pages
        Logger::file("      error_pages: {");
        for (const auto& error : server.errorPages)
            Logger::file("         " + std::to_string(error.first) + ": " + error.second);
        Logger::file("      }");

        // Log locations
        //Logger::file("      locations: [");
        //for (const auto& loc : server.locations) {
        //    Logger::file("         {");
        //    Logger::file("            port: " + std::to_string(loc.port));
        //    Logger::file("            path: " + loc.path);
        //    Logger::file("            methods: " + loc.methods);
        //    Logger::file("            autoindex: " + loc.autoindex);
        //    Logger::file("            default_file: " + loc.default_file);
        //    Logger::file("            upload_store: " + loc.upload_store);
        //    Logger::file("            client_max_body_size: " + std::to_string(loc.client_max_body_size));
        //    Logger::file("            root: " + loc.root);
        //    Logger::file("            cgi: " + loc.cgi);
        //    Logger::file("            cgi_filetype: " + loc.cgi_filetype);
        //    Logger::file("            return_code: " + loc.return_code);
        //    Logger::file("            return_url: " + loc.return_url);
        //    Logger::file("            doAutoindex: " + std::string(loc.doAutoindex ? "true" : "false"));
        //    Logger::file("            allowGet: " + std::string(loc.allowGet ? "true" : "false"));
        //    Logger::file("            allowPost: " + std::string(loc.allowPost ? "true" : "false"));
        //    Logger::file("            allowDelete: " + std::string(loc.allowDelete ? "true" : "false"));
        //    Logger::file("            allowCookie: " + std::string(loc.allowCookie ? "true" : "false"));
        //    Logger::file("         }");
        //}
        Logger::file("      ]");
        Logger::file("   }");
	}

    Logger::file("]\n");
}

// Returns a string description of an epoll event based on its event flags
std::string getEventDescription(uint32_t ev)
{
    std::ostringstream description;

    if (ev & EPOLLIN)
        description << "EPOLLIN ";
    if (ev & EPOLLOUT)
        description << "EPOLLOUT ";
    if (ev & EPOLLHUP)
        description << "EPOLLHUP ";
    if (ev & EPOLLERR)
        description << "EPOLLERR ";
    if (ev & EPOLLRDHUP)
        description << "EPOLLRDHUP ";
    if (ev & EPOLLPRI)
        description << "EPOLLPRI ";
    if (ev & EPOLLET)
        description << "EPOLLET ";
    if (ev & EPOLLONESHOT)
        description << "EPOLLONESHOT ";

    // Remove the trailing space if there's any description
    std::string result = description.str();
    if (!result.empty() && result.back() == ' ')
        result.pop_back();

    return result.empty() ? "UNKNOWN EVENT" : result;
}

// Logs global file descriptors, including epoll FD and client-to-server FD mappings
void log_global_fds(const GlobalFDS& fds)
{
	Logger::file("GlobalFDS clFD_to_svFD_map:");
	Logger::file("epoll_fd: " + std::to_string(fds.epoll_fd));

	Logger::file("[");
	if (fds.clFD_to_svFD_map.empty())
		Logger::file("   empty");
	for (const auto& pair : fds.clFD_to_svFD_map)
		Logger::file("   client_fd: " + std::to_string(pair.first) + " -> server_fd: " + std::to_string(pair.second));
	Logger::file("]\n");
}

// Prints server block details such as port, server FD, root, index, and error pages
void printServerBlock(ServerBlock& serverBlock)
{
	std::cout << "---- ServerBlock Information ----" << std::endl;

	std::cout << "Port: " << serverBlock.port << std::endl;
	std::cout << "Server FD: " << serverBlock.server_fd << std::endl;
	std::cout << "Name: " << serverBlock.name << std::endl;
	std::cout << "Root: " << serverBlock.root << std::endl;
	std::cout << "Index: " << serverBlock.index << std::endl;
	std::cout << "Client Max Body Size: " << serverBlock.client_max_body_size << std::endl;

	std::cout << "Error Pages:" << std::endl;
	for (std::map<int, std::string>::const_iterator it = serverBlock.errorPages.begin();
		it != serverBlock.errorPages.end(); ++it)
	{
		std::cout << "  Error Code: " << it->first << ", Page: " << it->second << std::endl;
	}

	std::cout << "Locations:" << std::endl;
	for (size_t i = 0; i < serverBlock.locations.size(); ++i)
	{
		std::cout << "  Location " << i + 1 << ":" << std::endl;
		std::cout << "    (Add Location details here)" << std::endl;
	}

	std::cout << "---------------------------------" << std::endl;
}

// Prints request body information, including client FD, CGI state, request and response buffers
void printRequestBody(const Context& ctx)
{
	std::cout << "---- RequestBody Information ----" << std::endl;
	std::cout << "Client FD: " << ctx.client_fd << std::endl;
	std::cout << "CGI Input FD: " << ctx.req.cgi_in_fd << std::endl;
	std::cout << "CGI Output FD: " << ctx.req.cgi_out_fd << std::endl;
	std::cout << "CGI PID: " << ctx.req.cgi_pid << std::endl;
	std::cout << "CGI Done: " << (ctx.req.cgi_done ? "true" : "false") << std::endl;
	std::cout << "State: ";
	switch (ctx.req.state)
	{
	case RequestBody::STATE_READING_REQUEST:
		std::cout << "STATE_READING_REQUEST";
		break;
	case RequestBody::STATE_PREPARE_CGI:
		std::cout << "STATE_PREPARE_CGI";
		break;
	case RequestBody::STATE_CGI_RUNNING:
		std::cout << "STATE_CGI_RUNNING";
		break;
	case RequestBody::STATE_SENDING_RESPONSE:
		std::cout << "STATE_SENDING_RESPONSE";
		break;
	default:
		std::cout << "UNKNOWN";
		break;
	}
	std::cout << std::endl;
	std::cout << "Request Buffer Size: " << ctx.req.request_buffer.size() << std::endl;
	if (!ctx.req.request_buffer.empty())
	{
		std::cout << "Request Buffer Content (first 50 chars): "
			<< std::string(ctx.req.request_buffer.begin(),
			ctx.req.request_buffer.end())
			<< std::endl;
	}
	std::cout << "Response Buffer Size: " << ctx.req.response_buffer.size() << std::endl;
	if (!ctx.req.response_buffer.empty())
	{
		std::cout << "Response Buffer Content (first 50 chars): "
			<< std::string(ctx.req.response_buffer.begin(), ctx.req.response_buffer.end())
			<< std::endl;
	}
	std::cout << "CGI Output Buffer Size: " << ctx.req.cgi_output_buffer.size() << std::endl;
	if (!ctx.req.cgi_output_buffer.empty())
	{
		std::cout << "CGI Output Buffer Content (first 50 chars): "
			<< std::string(ctx.req.cgi_output_buffer.begin(), ctx.req.cgi_output_buffer.end())
			<< std::endl;
	}
	if (ctx.req.associated_conf)
	{
		std::cout << "Associated ServerBlock:" << std::endl;
		printServerBlock(*ctx.req.associated_conf);
	}
	else
		std::cout << "Associated ServerBlock: NULL" << std::endl;
	std::cout << "Requested Path: " << ctx.req.requested_path << std::endl;
	std::cout << "----------------------------------" << std::endl;
}
