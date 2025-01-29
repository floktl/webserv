#include "./Logger.hpp"

void Logger::red(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), RED, newline);
}

void Logger::green(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), GREEN, newline);
}

void Logger::blue(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), BLUE, newline);
}

void Logger::yellow(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), YELLOW, newline);
}

void Logger::cyan(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), CYAN, newline);
}

void Logger::magenta(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), MAGENTA, newline);
}

void Logger::white(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), WHITE, newline);
}

void Logger::black(const std::string &message, bool newline, size_t length) {
	log(formatMessage(message, length), BLACK, newline);
}

void Logger::file(const std::string& message) {
	try {

		std::string filePath = "./webserv.log";
		std::ofstream logfile(filePath.c_str(), std::ios::app);
		if (!logfile.is_open()) {
			throw std::runtime_error("Konnte Logdatei nicht öffnen");
		}

		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&time), "[%Y-%m-%d %H:%M:%S]");

		logfile << ss.str() << " " << message << std::endl;
	} catch (const std::exception& e) {
	}
}

void Logger::errorLog(const std::string& message) {
	try {
		// File logging
		std::string filePath = "./webserv.log";
		std::ofstream logfile(filePath.c_str(), std::ios::app);
		if (!logfile.is_open()) {
			throw std::runtime_error("Could not open log file");
		}

		// Get the current timestamp
		auto now = std::chrono::system_clock::now();
		auto time = std::chrono::system_clock::to_time_t(now);
		std::stringstream ss;
		ss << std::put_time(std::localtime(&time), "[%Y-%m-%d %H:%M:%S] 8===D ");

		// Write plain text "[ERROR]" to the log file
		logfile << ss.str() << " [ERROR] " << message << std::endl;

		// Terminal logging with red "[ERROR]"
		std::cerr << ss.str() << " " << RED << "[ERROR]" << RESET << " " << message << std::endl;

	} catch (const std::exception& e) {
		// Fallback in case of an error
		std::cerr << RED << "[ERROR]" << RESET << " Logging failed: " << e.what() << std::endl;
	}
}



std::string Logger::formatMessage(const std::string &message, size_t length) {
	if (message.length() >= length) {
		return message;
	}
	return message + std::string(length - message.length(), ' ');
}

void Logger::log(const std::string &message, const std::string &color, bool newline) {
	std::cout << color << message << RESET;
	if (newline)
		std::cout << std::endl;
}

void Logger::error(const std::string &message, bool newline) {
	std::cerr << RED << message << RESET;
	if (newline)
		std::cerr << std::endl;
}

Logger::StreamLogger::StreamLogger(const std::string &colorParam, bool newlineParam, bool useCerrParam)
	: color(colorParam), newline(newlineParam), useCerr(useCerrParam) {}

Logger::StreamLogger::~StreamLogger() {
	flush();
}

void Logger::StreamLogger::flush() {
	if (useCerr)
		std::cerr << color << buffer.str() << RESET;
	else
		std::cout << color << buffer.str() << RESET;

	if (newline) {
		if (useCerr)
			std::cerr << std::endl;
		else
			std::cout << std::endl;
	}
}

Logger::StreamLogger &Logger::red() {
	static StreamLogger logger(RED);
	return logger;
}

Logger::StreamLogger &Logger::green() {
	static StreamLogger logger(GREEN);
	return logger;
}

Logger::StreamLogger &Logger::blue() {
	static StreamLogger logger(BLUE);
	return logger;
}

Logger::StreamLogger &Logger::yellow() {
	static StreamLogger logger(YELLOW);
	return logger;
}

Logger::StreamLogger &Logger::cyan() {
	static StreamLogger logger(CYAN);
	return logger;
}

Logger::StreamLogger &Logger::magenta() {
	static StreamLogger logger(MAGENTA);
	return logger;
}

Logger::StreamLogger &Logger::white() {
	static StreamLogger logger(WHITE);
	return logger;
}

Logger::StreamLogger &Logger::black() {
	static StreamLogger logger(BLACK);
	return logger;
}

Logger::StreamLogger &Logger::error() {
	static StreamLogger logger(RED, true, true);
	return logger;
}

Logger::StreamLogger& Logger::file() {
	static StreamLogger logger("", true, false);
	return logger;
}

