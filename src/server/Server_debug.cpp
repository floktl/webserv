#include "Server.hpp"

// Logs Detail Context Information, Including FDS, Request Type, Headers, and Activity Timestamp
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


// Logs Server Configuration Details, Including Ports, Root Paths, Timeouts, and Error Pages
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

		Logger::file("      ]");
		Logger::file("   }");
	}

	Logger::file("]\n");
}

// Logs Global File Descriptor, including Epoll FD and Client-to-Server FD Mappings
void log_global_fds(const GlobalFDS& fds)
{
	Logger::file("GlobalFDS Snapshot:");
	Logger::file("epoll_fd: " + std::to_string(fds.epoll_fd));

	// clFD_to_svFD_map
	Logger::file("\nclFD_to_svFD_map:");
	Logger::file("[");
	if (fds.clFD_to_svFD_map.empty())
		Logger::file("   empty");
	for (const auto& pair : fds.clFD_to_svFD_map)
		Logger::file("   client_fd: " + std::to_string(pair.first) + " -> server_fd: " + std::to_string(pair.second));
	Logger::file("]");

	// cgi_pipe_to_client_fd
	Logger::file("\ncgi_pipe_to_client_fd:");
	Logger::file("[");
	if (fds.cgi_pipe_to_client_fd.empty())
		Logger::file("   empty");
	for (const auto& pair : fds.cgi_pipe_to_client_fd)
		Logger::file("   cgi_pipe_fd: " + std::to_string(pair.first) + " -> client_fd: " + std::to_string(pair.second));
	Logger::file("]");

	// cgi_pid_to_client_fd
	Logger::file("\ncgi_pid_to_client_fd:");
	Logger::file("[");
	if (fds.cgi_pid_to_client_fd.empty())
		Logger::file("   empty");
	for (const auto& pair : fds.cgi_pid_to_client_fd)
		Logger::file("   cgi_pid: " + std::to_string(pair.first) + " -> client_fd: " + std::to_string(pair.second));
	Logger::file("]\n");
}


// Prints server block details search as port, server FD, root, index, and error pages
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
		std::cout <<  serverBlock.locations[i].root << std::endl;
		std::cout <<  serverBlock.locations[i].path << std::endl;
		std::cout <<  serverBlock.locations[i].methods << std::endl;
		std::cout <<  serverBlock.locations[i].return_url << std::endl;
		std::cout <<  serverBlock.locations[i].return_code << std::endl;
		std::cout << "    (Add Location details here)" << std::endl;
	}

	std::cout << "---------------------------------" << std::endl;
}

// Prints Request Body Information, Including Client FD, CGI State, Request and Response Buffers
void printRequestBody(const Context& ctx)
{
	std::cout << "---- RequestBody Information ----" << std::endl;
	std::cout << "Client FD: " << ctx.client_fd << std::endl;
	std::cout << "CGI Input FD: " << ctx.req.cgi_in_fd << std::endl;
	std::cout << "CGI Output FD: " << ctx.req.cgi_out_fd << std::endl;
	std::cout << "CGI PID: " << ctx.req.cgi_pid << std::endl;
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
	else
		std::cout << "Associated ServerBlock: NULL" << std::endl;
	std::cout << "----------------------------------" << std::endl;
}
