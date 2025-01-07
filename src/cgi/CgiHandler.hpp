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
		//setup functions
		CgiHandler(Server& server);
		~CgiHandler();

		// execution functions
		void handleCGIWrite(int epfd, int fd, uint32_t events);
		void handleCGIRead(int epfd, int fd);
		void addCgiTunnel(RequestState& req, const std::string& method, const std::string& query);
		bool needsCGI(RequestState &req, const std::string &path);

		// cleanup functions
		void cleanupCGI(RequestState &req);

	private:
		Server& server;
		std::map<int, CgiTunnel*> fd_to_tunnel;
		std::map<int, CgiTunnel> tunnels;

		// execution functions
		void execute_cgi(const CgiTunnel& tunnel);
		// cleanup functions
		void cleanup_tunnel(CgiTunnel& tunnel);
		void cleanup_pipes(int pipe_in[2], int pipe_out[2]);

		// setup functions
		void setup_cgi_environment(const CgiTunnel& tunnel, const std::string& method, const std::string& query);
		bool initTunnel(RequestState &req, CgiTunnel &tunnel, int pipe_in[2],
			int pipe_out[2]);
		void handleChildProcess(int pipe_in[2], int pipe_out[2], CgiTunnel &tunnel, const std::string &method, const std::string &query);

};

#endif