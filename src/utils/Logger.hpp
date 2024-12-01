/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Logger.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 12:39:04 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 13:16:11 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <sstream>
#include <string>

#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define BLUE "\033[0;34m"
#define YELLOW "\033[0;93m"
#define CYAN "\033[0;36m"
#define MAGENTA "\033[0;35m"
#define WHITE "\033[0;97m"
#define RESET "\033[0m"

class Logger {
	private:
		static std::string formatMessage(const std::string &message, size_t length);

	public:
		static void red(const std::string &message, bool newline = true, size_t length = 0);
		static void green(const std::string &message, bool newline = true, size_t length = 0);
		static void blue(const std::string &message, bool newline = true, size_t length = 0);
		static void yellow(const std::string &message, bool newline = true, size_t length = 0);
		static void cyan(const std::string &message, bool newline = true, size_t length = 0);
		static void magenta(const std::string &message, bool newline = true, size_t length = 0);
		static void white(const std::string &message, bool newline = true, size_t length = 0);
		static void error(const std::string &message, bool newline = true);

		static void log(const std::string &message, const std::string &color, bool newline = true);

		class StreamLogger {
			public:
				StreamLogger(const std::string &color, bool newline = true, bool useCerr = false);
				~StreamLogger();
				template <typename T>
				StreamLogger &operator<<(const T &data) {
					buffer << data;
					return *this;
				}
				void flush();
			private:
				std::ostringstream buffer;
				std::string color;
				bool newline;
				bool useCerr;
				StreamLogger(const StreamLogger &);
				StreamLogger &operator=(const StreamLogger &);
		};

		static StreamLogger &red();
		static StreamLogger &green();
		static StreamLogger &blue();
		static StreamLogger &yellow();
		static StreamLogger &cyan();
		static StreamLogger &magenta();
		static StreamLogger &white();
		static StreamLogger &error();
};

#endif