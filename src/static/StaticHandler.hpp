#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "../main.hpp"

class Server;

class StaticHandler
{
private:
    // Represents the state of a file being served to a client
    struct FileState {
        std::ifstream file;
        size_t remaining_bytes;
        bool headers_sent;
    };

    std::map<int, FileState> file_states;
    Server& server;

public:
    StaticHandler(Server& server);

    // Handles client read events, for receiving HTTP requests
    void handleClientRead(int epfd, int fd);

    // Handles client write events, for sending HTTP responses or files
    void handleClientWrite(int epfd, int fd);
};

#endif