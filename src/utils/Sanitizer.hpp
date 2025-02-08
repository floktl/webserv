#ifndef SANITIZER_HPP
# define SANITIZER_HPP

#include "../server/server.hpp"

class Sanitizer {
	private:
		static bool isValidPath(std::string& path, const std::string& context, const std::string& pwd);
		static bool isValidFilename(const std::string& filename, bool allowPath);
		static bool createDirectoryWithPermissions(const std::string& path);
		static bool isValidUploadPath(std::string& path, const std::string& context);

	public:
		Sanitizer();
		~Sanitizer();
		static long parseSize(const std::string& sizeStr, const std::string& unit);
		static bool sanitize_portNr(int portNr);
		static bool sanitize_serverName(std::string& serverName);
		static bool sanitize_root(std::string& root, const std::string& pwd);
		static bool sanitize_index(std::string& index);
		static bool sanitize_errorPage(std::string& errorPage, const std::string& pwd);
		static bool sanitize_clMaxBodSize(long clMaxBodSize);
		static bool sanitize_timeout(int timeout);
		static bool sanitize_locationPath(std::string& locationPath, const std::string& pwd);
		static bool sanitize_locationMethods(std::string& locationMethods);
		static bool sanitize_locationReturn(std::string& locationReturn);
		static bool sanitize_locationRoot(std::string& locationRoot, const std::string& pwd);
		static bool sanitize_locationAutoindex(std::string& locationAutoindex, bool &doAutoindex);
		static bool sanitize_locationDefaultFile(std::string& locationDefaultFile);
		static bool sanitize_locationUploadStore(std::string& locationUploadStore, const std::string& pwd, const std::string& serverRoot, const std::string& locationRoot);
		static bool sanitize_locationClMaxBodSize(long locationClMaxBodSize);
		static bool sanitize_locationCgi(std::string& locationCgi, std::string& locationCgiFileType, const std::string& pwd);
		static bool sanitize_locationCgiParam(std::string& locationCgiParam);
		static bool sanitize_locationRedirect(std::string& locationRedirect);
};

#endif