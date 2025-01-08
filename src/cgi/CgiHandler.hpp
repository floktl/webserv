#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "../main.hpp"

struct ServerBlock;
struct RequestState;
struct Location;
struct CgiTunnel;

class Server;

class CgiHandler
{
	public:
		CgiHandler(Server& server);
		~CgiHandler();
		void finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd);

		void handleCGIWrite(int epfd, int fd, uint32_t events);
		void handleCGIRead(int epfd, int fd);
		void addCgiTunnel(RequestState& req, const std::string& method, const std::string& query);
		bool needsCGI(RequestState &req, const std::string &path);

		void cleanupCGI(RequestState &req);

	private:
		Server& server;
		std::map<int, CgiTunnel> tunnels;
		std::map<int, CgiTunnel*> fd_to_tunnel;

		void execute_cgi(const CgiTunnel& tunnel);

		void cleanup_tunnel(CgiTunnel& tunnel);
		void cleanup_pipes(int pipe_in[2], int pipe_out[2]);

		void setup_cgi_environment(const CgiTunnel& tunnel, const std::string& method, const std::string& query);
		bool initTunnel(RequestState &req, CgiTunnel &tunnel, int pipe_in[2],
			int pipe_out[2]);
		void handleChildProcess(int pipe_in[2], int pipe_out[2], CgiTunnel &tunnel,
			const std::string &method, const std::string &query);


};

#endif