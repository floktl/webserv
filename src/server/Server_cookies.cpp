#include "Server.hpp"

void Server::parseCookies(Context& ctx, std::string value) {
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
}


std::string Server::generateSetCookieHeader(const Cookie& cookie) {
	std::stringstream ss;
	ss << "Set-Cookie: " << cookie.name << "=" << cookie.value;

	if (!cookie.path.empty())
		ss << "; Path=" << cookie.path;

	if (cookie.expires > 0) {
		char timeStr[100];
		struct tm* tm = gmtime(&cookie.expires);
		strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S GMT", tm);
		ss << "; Expires=" << timeStr;
	}

	if (cookie.secure)
		ss << "; Secure";

	if (cookie.httpOnly)
		ss << "; HttpOnly";

	return ss.str();
}

