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
		void addCgiTunnel(RequestState& req, const std::string& method, const std::string& query);
		void handleCGIWrite(int epfd, int fd, uint32_t events);
		void handleCGIRead(int epfd, int fd);
		const Location* findMatchingLocation(const ServerBlock* conf, const std::string& path);
		bool needsCGI(const ServerBlock* conf, const std::string& path);
		void cleanupCGI(RequestState &req);
	private:
		Server& server;
		std::map<int, CgiTunnel*> fd_to_tunnel;
		std::map<int, CgiTunnel> tunnels;
		void cleanup_tunnel(CgiTunnel& tunnel);
		void setup_cgi_environment(const CgiTunnel& tunnel, const std::string& method, const std::string& query);
		void execute_cgi(const CgiTunnel& tunnel);
		void cleanup_pipes(int pipe_in[2], int pipe_out[2]);
};


#endif