# Webserv

**This is when you finally understand why a URL starts with HTTP.**

## Summary

Webserv is a project to build a fully functional HTTP server in C++98. It supports multiple features such as handling HTTP requests (GET, POST, DELETE), serving static files, and processing CGI scripts. The server must be compatible with modern web browsers and resilient under stress testing.

## Mandatory Features

### General Requirements
- Implemented in C++98 with a focus on HTTP/1.1 compliance.
- Must use non-blocking I/O with `poll()` or equivalent for all client-server interactions.
- No external libraries allowed, except for the standard C++ library and specified system calls.
- Default error pages must be served if none are provided.
- Should support:
  - Multiple ports.
  - HTTP methods: GET, POST, DELETE.
  - File uploads.
  - Serving static websites.
  - Default configuration and custom configuration files.

### Configuration File
- Specify ports and hosts for each server.
- Define server names and default error pages.
- Set client body size limits.
- Configure routes:
  - Define allowed HTTP methods.
  - Set HTTP redirections.
  - Serve specific files or directories.
  - Enable or disable directory listing.
  - Set default files for directory requests.
  - Execute CGI scripts for specific file extensions.
  - Handle file uploads and configure save locations.

### Resilience
- Must not crash under any circumstances.
- Should handle invalid inputs gracefully and provide appropriate error messages.
- Stress-tested for robustness.

## Bonus Features
- Support cookies and session management.
- Handle multiple CGI scripts.
- Additional advanced HTTP/1.1 features.

## Implementation Details

### Compilation
- Provide a `Makefile` with the following rules:
  - `all`: Compile the project.
  - `clean`: Remove object files.
  - `fclean`: Remove all generated files.
  - `re`: Recompile the project.
- Compile with:
```bash
c++ -Wall -Wextra -Werror -std=c++98
```

## Usage
Run the server:
```
./webserv [configuration file]
```
Example configuration includes:
- Listening on multiple ports.
- Specifying root directories for file serving.
- Custom error pages and body size limits.
- CGI setup (e.g., PHP, Python scripts).

### Testing
- Compare behavior with NGINX to ensure HTTP/1.1 compliance.
- Use tools like telnet or custom scripts (Python, Golang, etc.) to test responses.
- Evaluate resilience with stress tests and edge cases.

## Learning Objectives
- Master HTTP protocol fundamentals.
- Gain deep insights into socket programming and non-blocking I/O.
- Implement a robust, multi-threaded server capable of handling real-world scenarios.
- Understand CGI and its interaction with HTTP servers.

### Submission
Submit all source files, configuration files, and a comprehensive Makefile in your Git repository.
Ensure every feature works perfectly before attempting the bonus part.

Let your server handle the web like a pro!