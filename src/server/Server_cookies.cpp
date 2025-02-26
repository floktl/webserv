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

	if (!cookie.path.empty()) {
		ss << "; Path=" << cookie.path;
	}

	if (cookie.expires > 0) {
		time_t remaining = cookie.expires;

		const time_t seconds_per_day = 86400;
		const time_t seconds_per_hour = 3600;
		const time_t seconds_per_minute = 60;

		time_t days = remaining / seconds_per_day;
		remaining %= seconds_per_day;

		time_t hours = remaining / seconds_per_hour;
		remaining %= seconds_per_hour;
		time_t minutes = remaining / seconds_per_minute;
		time_t seconds = remaining % seconds_per_minute;

		int year = 1970;
		const int days_per_year = 365;
		const int days_per_leap_year = 366;

		while (days >= (year % 4 == 0 ? days_per_leap_year : days_per_year)) {
			days -= (year % 4 == 0 ? days_per_leap_year : days_per_year);
			year++;
		}

		const int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

		int month = 0;
		if (year % 4 == 0 && month == 1) {
			if (days >= 29) {
				days -= 29;
				month++;
			}
		} else {
			while (month < 12 && days >= days_in_month[month]) {
				days -= days_in_month[month];
				month++;
			}
		}

		const std::string month_names[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun",
			"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};

		const std::string day_names[] = {
			"Thu", "Fri", "Sat", "Sun", "Mon", "Tue", "Wed"
		};

		time_t total_days = cookie.expires / seconds_per_day;
		int day_of_week = total_days % 7;

		ss << "; Expires=" << day_names[day_of_week] << ", "
		<< std::setfill('0') << std::setw(2) << (days + 1) << " "
		<< month_names[month] << " "
		<< year << " "
		<< std::setfill('0') << std::setw(2) << hours << ":"
		<< std::setfill('0') << std::setw(2) << minutes << ":"
		<< std::setfill('0') << std::setw(2) << seconds << " GMT";
	}

	return ss.str();
}
