#include "server.hpp"

// Extracts the hostname from the http host header, return on empty string if missing
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

// Adds a server name Entry to the /etc /hosts file
bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	while (std::getline(infile, line))
	{
		if (line.find(server_name) != std::string::npos)
			return true;
	}

	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open())
		throw std::runtime_error("Failed to open /etc/hosts");
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();
	Logger::yellow("Added " + server_name + " to /etc/hosts file");
	added_server_names.push_back(server_name);
	return true;
}

// Removes Previously Added Server Names from the /etc /hosts file
void Server::removeAddedServerNamesFromHosts()
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	if (!infile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(infile, line))
	{
		bool shouldRemove = false;
		for (const auto &name : added_server_names)
		{
			if (line.find(name) != std::string::npos)
			{
				Logger::yellow("Remove " + name + " from /etc/host file");
				shouldRemove = true;
				break;
			}
		}
		if (!shouldRemove)
			lines.push_back(line);
	}
	infile.close();
	std::ofstream outfile(hosts_file, std::ios::trunc);
	if (!outfile.is_open())
		throw std::runtime_error("Failed to open /etc/hosts for writing");
	for (const auto &l : lines)
		outfile << l << "\n";
	outfile.close();
	added_server_names.clear();
}
