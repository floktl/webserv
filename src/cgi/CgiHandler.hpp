#pragma once

#include "../server/server.hpp"

struct ServerBlock;
struct RequestBody;
struct Location;
struct CgiTunnel;

class Server;

class CgiHandler
{
	public:
		CgiHandler(Server& server);
		~CgiHandler();
		void finalizeCgiResponse(RequestBody &req, int epoll_fd, int client_fd);

		void handleCGIWrite(int epfd, int fd, uint32_t events);
		void handleCGIRead(int epfd, int fd);
		void addCgiTunnel(RequestBody& req, const std::string& method, const std::string& query);
		bool needsCGI(RequestBody &req, const std::string &path);

		void cleanupCGI(RequestBody &req);

	private:
		Server& server;
		std::map<int, CgiTunnel> tunnels;
		std::map<int, CgiTunnel*> fd_to_tunnel;


		void cleanup_tunnel(CgiTunnel& tunnel);
		void cleanup_pipes(int pipe_in[2], int pipe_out[2]);

		std::vector<char*> setup_cgi_environment(const CgiTunnel& tunnel, const std::string& method, const std::string& query);
		bool initTunnel(RequestBody &req, CgiTunnel &tunnel, int pipe_in[2],
			int pipe_out[2]);
		void handleChildProcess(int pipe_in[2], int pipe_out[2], CgiTunnel &tunnel,
			const std::string &method, const std::string &query);
		void free_environment(std::vector<char*>& env);
};
