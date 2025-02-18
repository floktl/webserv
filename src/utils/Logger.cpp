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

bool Logger::errorLog(const std::string& message)
{
	try
	{
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
		ss << std::put_time(std::localtime(&time), "[%Y-%m-%d %H:%M:%S] ");

		// Write plain text "[ERROR]" to the log file
		logfile << ss.str() << " [ERROR] " << message << std::endl;

		// Terminal logging with red "[ERROR]"
		std::cerr << ss.str() << " " << RED << "[ERROR]" << RESET << " " << message << std::endl;
		return false;

	} catch (const std::exception& e) {
		// Fallback in case of an error
		std::cerr << RED << "[ERROR]" << RESET << " Logging failed: " << e.what() << std::endl;
		return false;
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

bool Logger::error(const std::string &message, bool newline)
{
	std::cerr << RED << message << RESET;
	if (newline)
		std::cerr << std::endl;
	return false;
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

void Logger::progressBar(long long current, long long total, const std::string& prefix) {

	if (current > 0) {
		const int barWidth = 20;
		float progress = total > 0 ? (float)current / total : 0.0f;
		int pos = barWidth * progress;

		std::cout << "\r" << prefix;
		std::cout << "";

		for (int i = 0; i < barWidth; ++i) {
			if (i < barWidth - 1) {
				if (i < pos) {
					std::cout << "=";
				}
				else if (i == pos && progress < 1.0f) {
					std::cout << "D";
				}
				else {
					std::cout << " ";
				}
			}
			else {
				if (progress >= 1.0f) {
					std::cout << "D";
				}
				else if (i < pos) {
					std::cout << "=";
				}
				else {
					std::cout << " ";
				}
			}
		}

		int percent = progress * 100.0;
		std::cout << " " << percent << "% ";
		std::cout.flush();
	}
}

std::string Logger::logReadBuffer(const std::string& buffer) {
	const size_t maxDisplayBytes = 100;

	if (buffer.size() <= maxDisplayBytes * 2) {
		return buffer;
	} else {
		std::string result = buffer.substr(0, maxDisplayBytes);
		result += "\n[... " + std::to_string(buffer.size() - maxDisplayBytes * 2) + " bytes omitted ...]\n";
		result += buffer.substr(buffer.size() - maxDisplayBytes);
		return result;
	}
}

std::string Logger::logWriteBuffer(const std::vector<char>& buffer) {
	const size_t maxDisplayBytes = 100;

	if (buffer.size() <= maxDisplayBytes * 2) {
		return std::string(buffer.begin(), buffer.end());
	} else {
		std::string result(buffer.begin(), buffer.begin() + maxDisplayBytes);
		result += "\n[... " + std::to_string(buffer.size() - maxDisplayBytes * 2) + " bytes omitted ...]\n";
		result += std::string(buffer.end() - maxDisplayBytes, buffer.end());
		return result;
	}
}