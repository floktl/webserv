#include "Server.hpp"

// Parses existing "Cookie" header values and stores them in the request context
void Server::parseCookies(Context& ctx, std::string value) {
    Logger::file("parseCookies input: " + value);

    size_t start = 0;
    size_t pos;

    while ((pos = value.find(';', start)) != std::string::npos) {
        std::string cookiePair = value.substr(start, pos - start);
        cookiePair = trim(cookiePair);

        size_t equalPos = cookiePair.find('=');
        if (equalPos != std::string::npos) {
            std::string name = trim(cookiePair.substr(0, equalPos));
            std::string cookieValue = trim(cookiePair.substr(equalPos + 1));

            if (!cookieValue.empty() && cookieValue.front() == '"')
                cookieValue = cookieValue.substr(1);
            if (!cookieValue.empty() && cookieValue.back() == '"')
                cookieValue = cookieValue.substr(0, cookieValue.length() - 1);

            ctx.cookies.push_back(std::make_pair(name, cookieValue));
        }

        start = pos + 1;
    }

    if (start < value.length()) {
        std::string cookiePair = trim(value.substr(start));
        size_t equalPos = cookiePair.find('=');
        if (equalPos != std::string::npos) {
            std::string name = trim(cookiePair.substr(0, equalPos));
            std::string cookieValue = trim(cookiePair.substr(equalPos + 1));

            if (!cookieValue.empty() && cookieValue.front() == '"')
                cookieValue = cookieValue.substr(1);
            if (!cookieValue.empty() && cookieValue.back() == '"')
                cookieValue = cookieValue.substr(0, cookieValue.length() - 1);

            ctx.cookies.push_back(std::make_pair(name, cookieValue));
        }
    }

    for (const auto& cookie : ctx.cookies) {
        Logger::file("Parsed cookie: " + cookie.first + "=" + cookie.second);
    }
}