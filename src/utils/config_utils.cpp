#include "../main.hpp"

// internal trimming
std::string trim(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t");
	return str.substr(first, last - first + 1);
}

bool	updateErrorStatus(Context &ctx, int error_code, std::string error_string)
{
	ctx.type = ERROR;
	ctx.error_code = error_code;
	ctx.error_message = error_string;
	return false;
}

// parsing constants
std::vector<std::string> parseOptionsToVector(const std::string& opts)
{
	std::vector<std::string> result;
	std::istringstream stream(opts);
	std::string opt;
	while (std::getline(stream, opt, ','))
	{
		result.push_back(trim(opt));
	}
	return result;
}

// expand ENV variables in bash style for config values
std::string expandEnvironmentVariables(const std::string& value, char** env)
{
	if (!env) return value;

	std::string result = value;
	size_t pos = 0;

	while ((pos = result.find('$', pos)) != std::string::npos)
	{
		if (pos + 1 >= result.length()) break;

		size_t end = pos + 1;
		while (end < result.length() &&
			(std::isalnum((unsigned char)result[end]) || result[end] == '_'))
		{
			end++;
		}

		std::string varName = result.substr(pos + 1, end - (pos + 1));

		bool found = false;
		for (char** envVar = env; *envVar != nullptr; envVar++)
		{
			std::string envStr(*envVar);
			size_t equalPos = envStr.find('=');
			if (equalPos != std::string::npos)
			{
				std::string name = envStr.substr(0, equalPos);
				if (name == varName)
				{
					std::string replacer = envStr.substr(equalPos + 1);
					result.replace(pos, end - pos, replacer);
					pos += replacer.length();
					found = true;
					break;
				}
			}
		}

		if (!found)
		{
			pos = end;
		}
	}

	return result;
}

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
			<< std::string(ctx.req.response_buffer.begin(),
							ctx.req.response_buffer.end())
			<< std::endl;
	}

	std::cout << "CGI Output Buffer Size: " << ctx.req.cgi_output_buffer.size() << std::endl;
	if (!ctx.req.cgi_output_buffer.empty())
	{
		std::cout << "CGI Output Buffer Content (first 50 chars): "
			<< std::string(ctx.req.cgi_output_buffer.begin(),
							ctx.req.cgi_output_buffer.end())
			<< std::endl;
	}

	if (ctx.req.associated_conf)
	{
		std::cout << "Associated ServerBlock:" << std::endl;
		printServerBlock(*ctx.req.associated_conf);
	}
	else
	{
		std::cout << "Associated ServerBlock: NULL" << std::endl;
	}
	std::cout << "Requested Path: " << ctx.req.requested_path << std::endl;

	std::cout << "----------------------------------" << std::endl;
}
