#include "main.hpp"
#include <csignal>
#include <iostream>
#include <memory>
void log_server_configs(const std::vector<ServerBlock>& configs) {
    Logger::file("Server Configurations:");
    Logger::file("[");

    if (configs.empty()) {
        Logger::file("   empty");
    }

    for (const auto& server : configs) {
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
        for (const auto& error : server.errorPages) {
            Logger::file("         " + std::to_string(error.first) + ": " + error.second);
        }
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
std::string getEventDescription(uint32_t ev) {
    std::ostringstream description;

    if (ev & EPOLLIN) {
        description << "EPOLLIN ";
    }
    if (ev & EPOLLOUT) {
        description << "EPOLLOUT ";
    }
    if (ev & EPOLLHUP) {
        description << "EPOLLHUP ";
    }
    if (ev & EPOLLERR) {
        description << "EPOLLERR ";
    }
    if (ev & EPOLLRDHUP) {
        description << "EPOLLRDHUP ";
    }
    if (ev & EPOLLPRI) {
        description << "EPOLLPRI ";
    }
    if (ev & EPOLLET) {
        description << "EPOLLET ";
    }
    if (ev & EPOLLONESHOT) {
        description << "EPOLLONESHOT ";
    }

    // Remove the trailing space if there's any description
    std::string result = description.str();
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result.empty() ? "UNKNOWN EVENT" : result;
}

std::unique_ptr<Server> serverInstance;
void log_global_fds(const GlobalFDS& fds) {
    Logger::file("GlobalFDS clFD_to_svFD_map:");
    Logger::file("epoll_fd: " + std::to_string(fds.epoll_fd));

	Logger::file("[");
    if (fds.clFD_to_svFD_map.empty()) {
        Logger::file("   empty");
    }
    for (const auto& pair : fds.clFD_to_svFD_map) {
        Logger::file("   client_fd: " + std::to_string(pair.first) + " -> server_fd: " + std::to_string(pair.second));
    }
	Logger::file("]\n");
}
void handle_sigint(int sig)
{
	(void)sig;
	if (serverInstance)
	{
		Logger::cyan("\nCTRL+C received, shutting down gracefully...");
		serverInstance->cleanup();
		serverInstance.reset();  // Ensure cleanup
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	GlobalFDS globalFDS;
	serverInstance = std::make_unique<Server>(globalFDS);
	//ClientHandler clientHandler(*serverInstance);
	//CgiHandler cgiHandler(*serverInstance);

	// Set up signal handling
	struct sigaction action;
	action.sa_handler = handle_sigint;  // Register the function
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;  // No special flags needed
	Logger::yellow("Server Starting...");
	if (sigaction(SIGINT, &action, NULL) == -1)
	{
		Logger::red() << "Failed to set signal handler!";
		return EXIT_FAILURE;
	}

	try
	{
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid())
		{
			Logger::red() << "Invalid configuration file!";
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			return EXIT_FAILURE;
		}

		int epoll_fd = serverInstance->server_init(configs);
		if (epoll_fd == EXIT_FAILURE)
			return EXIT_FAILURE;

		if (serverInstance->runEventLoop(epoll_fd, configs) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

