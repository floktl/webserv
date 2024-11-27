/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 12:39:19 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 17:09:46 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Logger.hpp"

void Logger::red(const std::string &message, bool newline) {
	log(message, RED, newline);
}

void Logger::green(const std::string &message, bool newline) {
	log(message, GREEN, newline);
}

void Logger::blue(const std::string &message, bool newline) {
	log(message, BLUE, newline);
}

void Logger::yellow(const std::string &message, bool newline) {
	log(message, YELLOW, newline);
}

void Logger::cyan(const std::string &message, bool newline) {
	log(message, CYAN, newline);
}

void Logger::magenta(const std::string &message, bool newline) {
	log(message, MAGENTA, newline);
}

void Logger::white(const std::string &message, bool newline) {
	log(message, WHITE, newline);
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

Logger::StreamLogger &Logger::error() {
	static StreamLogger logger(RED, true, true);
	return logger;
}