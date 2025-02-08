#include "Server.hpp"

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

void Server::logRequestBodyMapFDs()
{
	if (globalFDS.context_map.empty())
		return;

	std::string log = "Active FDs: ";
	for (const auto& pair : globalFDS.context_map)
		log += std::to_string(pair.first) + " ";
}

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