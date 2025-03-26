import os
import re

# Define allowed external functions (system calls)
ALLOWED_FUNCTIONS = {
	"execve", "dup", "dup2", "pipe", "strerror", "gai_strerror", "errno",
	"fork", "socketpair", "htons", "htonl", "ntohs", "ntohl", "select", "poll",
	"epoll_create", "epoll_ctl", "epoll_wait", "kqueue", "kevent", "socket",
	"accept", "listen", "send", "recv", "chdir", "bind", "connect",
	"getaddrinfo", "freeaddrinfo", "setsockopt", "getsockname",
	"getprotobyname", "fcntl", "close", "read", "write", "waitpid", "kill",
	"signal", "access", "stat", "open", "opendir", "readdir", "closedir", "exit",
	"data", "at", "assign"
}

# C++ Keywords to Ignore (not functions)
CPP_KEYWORDS = {
	"if", "else", "switch", "case", "default", "break", "continue",
	"while", "do", "for", "try", "catch", "throw", "return",
	"new", "delete", "sizeof", "typeid", "dynamic_cast", "static_cast",
	"reinterpret_cast", "const_cast", "operator", "typedef", "namespace"
}

IGNORED_FUNCTIONS_AND_KEYWORDS = {
	# C++ Standard Library Functions
	"find", "find_first_not_of", "push_back", "find_last_not_of", "empty", "back",
	"begin", "end", "size", "erase", "substr", "clear", "insert", "replace", "c_str",
	"length", "rfind", "append", "count", "getline", "front", "tolower", "toupper",
	"compare", "swap", "str", "stream", "isdigit", "isalpha", "isalnum", "tolower",
	"toupper", "stoi", "stof", "stod", "atof", "atoi", "strtol", "strtod", "strtoul",

	# ✅ Standard Library Algorithms & Containers (C++17 Included)
	"find", "find_if", "find_if_not", "find_first_not_of", "find_last_not_of",
	"count", "count_if", "min", "max", "minmax", "minmax_element",
	"sort", "stable_sort", "partial_sort", "nth_element",
	"reverse", "reverse_copy", "rotate", "rotate_copy",
	"shuffle", "sample", "iota",
	"accumulate", "reduce", "transform_reduce", "inner_product",
	"adjacent_difference", "exclusive_scan", "inclusive_scan",
	"adjacent_find", "equal", "mismatch",
	"search", "search_n", "copy", "copy_if", "copy_n",
	"move", "move_backward", "swap", "swap_ranges",
	"partition", "stable_partition", "partition_point",
	"all_of", "any_of", "none_of", "for_each",
	"generate", "generate_n", "remove", "remove_if",
	"replace", "replace_if", "unique", "unique_copy",
	"sort_heap", "push_heap", "pop_heap", "make_heap",
	"clamp", "equal_range", "lower_bound", "upper_bound",
	"lexicographical_compare", "is_permutation", "next_permutation", "prev_permutation",

	# ✅ String Manipulation (std::string, std::string_view, etc.)
	"substr", "append", "replace", "c_str",
	"compare", "stream", "getline", "to_string",
	"stoi", "stol", "stoll", "stof", "stod", "stold",
	"strtol", "strtod", "strtoul", "strtoull",

	# ✅ Numeric Functions (C++17 included)
	"abs", "pow", "sqrt", "cbrt", "hypot", "log", "log10", "log1p", "exp",
	"ceil", "floor", "round", "trunc", "fmod", "modf",
	"isnan", "isinf", "isfinite", "copysign",
	"nextafter", "nexttoward", "remainder", "remquo",
	"fma", "fdim", "lerp",

	# ✅ Threading (C++17 included)
	"std::thread", "std::mutex", "std::timed_mutex", "std::recursive_mutex",
	"std::lock_guard", "std::unique_lock", "std::shared_lock",
	"std::condition_variable", "std::condition_variable_any",
	"std::future", "std::async", "std::promise", "std::packaged_task",

	# ✅ Exception Handling & Debugging
	"what", "catch", "throw", "try", "typeid",
	"std::exception", "std::bad_alloc", "std::bad_cast", "std::bad_typeid",

	# ✅ File I/O
	"std::ifstream", "std::ofstream", "std::fstream",
	"fstream::open", "ifstream::open", "ofstream::open",
	"is_open", "rdbuf", "seekg", "seekp", "tellg", "tellp",
	"eof", "good", "clear", "flush", "close",

	# ✅ Memory Management (C++17 included)
	"std::shared_ptr", "std::unique_ptr", "std::weak_ptr",
	"std::make_shared", "std::make_unique",
	"std::allocator", "std::align", "std::destroy_at",
	"std::hardware_constructive_interference_size", "std::hardware_destructive_interference_size",

	# ✅ Random Number Generation (C++17 included)
	"std::random_device", "std::mt19937", "std::mt19937_64",
	"std::uniform_int_distribution", "std::uniform_real_distribution",
	"std::normal_distribution", "std::bernoulli_distribution",
	"std::poisson_distribution", "std::binomial_distribution",
	"std::gamma_distribution", "std::geometric_distribution",

	# ✅ Regular Expressions (C++17 included)
	"std::regex", "std::regex_match", "std::regex_search",
	"std::regex_replace", "std::regex_constants",

	# ✅ Date and Time (C++17 included)
	"std::chrono", "std::chrono::system_clock", "std::chrono::steady_clock",
	"std::chrono::high_resolution_clock", "std::chrono::duration",
	"std::chrono::time_point", "std::chrono::milliseconds",
	"std::chrono::microseconds", "std::chrono::nanoseconds",

	# ✅ Miscellaneous Utilities (C++17 included)
	"std::any", "std::optional", "std::variant",
	"std::string_view", "std::array", "std::deque",
	"std::list", "std::map", "std::set", "std::unordered_map",
	"std::unordered_set", "std::vector",
	"std::byte", "std::data", "std::size", "std::as_const",

	# ✅ Color Formatting Functions (Logger)
	"cyan", "green", "magenta", "yellow", "white", "blue", "red", "black",
	"formatMessage", "useCerr", "logger", "log", "StreamLogger",

	# ✅ Webserver / CGI Specific
	"CgiHandler", "setup_cgi_environment",

	# ✅ Sanitizer Functions
	"Sanitizer", "sanitize", "sanitize_locationPath", "sanitize_index", "sanitize_root",
	"sanitize_locationCgiParam", "sanitize_locationReturn",
	"sanitize_locationMethods", "sanitize_portNr", "sanitize_serverName",
	"sanitize_errorPage", "sanitize_locationClMaxBodSize",
	"sanitize_locationUploadStore", "sanitize_locationRoot",
	"sanitize_timeout", "sanitize_locationCgi", "parseSize",
	"sanitize_locationAutoindex", "sanitize_locationDefaultFile", "isValidUploadPath",

	# ✅ Structs, Enums & Custom Types
	"ServerBlock", "processing", "timeout", "ConfigHandler", "reset", "error",
	"errorLog", "errorHandler", "Server", "ErrorHandler", "globalFDS", "color", "newline",
	"color", "pop_back", "Location", "printSize", "undefined", "getGlobalFds", "getTaskStatus",
	"server", "Content", "printIntValue", "printValue", "time_since_epoch", "formatSize",
	"Method", "find_last_of", "localtime", "strftime", "detected", "S_ISDIR", "S_ISREG",
	"g_shutdown_requested"
}

import os
import re

# Regex patterns for function detection
FUNCTION_DEF_PATTERN = re.compile(r"^\s*[a-zA-Z_][a-zA-Z0-9_:<>]*\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\(")
FUNCTION_CALL_PATTERN = re.compile(r"\b([a-zA-Z_][a-zA-Z0-9_:]*)\s*\(")
CLASS_METHOD_PATTERN = re.compile(r"\b([a-zA-Z_][a-zA-Z0-9_]*)::")
SINGLE_LINE_COMMENT = re.compile(r"^\s*//")
MULTI_LINE_COMMENT_START = re.compile(r"/\*")
MULTI_LINE_COMMENT_END = re.compile(r"\*/")
LOGGER_METHOD_PATTERN = re.compile(r"Logger::([a-zA-Z_][a-zA-Z0-9_]*)")

# Warning thresholds
MAX_FILE_LINES = 400
MAX_FILE_FUNCTIONS = 10
MAX_FUNCTION_LINES = 45

def find_defined_functions_and_classes(directory):
	"""Find all user-defined functions and classes in `.cpp` and `.hpp` files."""
	defined_functions = set()
	defined_classes = set()

	for root, _, files in os.walk(directory):
		for file in files:
			if file.endswith((".hpp", ".cpp")):
				file_path = os.path.abspath(os.path.join(root, file))
				with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
					in_multiline_comment = False
					for line in f:
						line = line.strip()

						# Handle multi-line comments
						if in_multiline_comment:
							if MULTI_LINE_COMMENT_END.search(line):
								in_multiline_comment = False
							continue
						if MULTI_LINE_COMMENT_START.search(line):
							in_multiline_comment = True
							continue

						# Skip single-line comments
						if SINGLE_LINE_COMMENT.match(line):
							continue

						# Detect function definitions
						match = FUNCTION_DEF_PATTERN.match(line)
						if match:
							function_name = line.split("(")[0].split()[-1]
							defined_functions.add(function_name)

						# Detect class method definitions (including Logger::<functionname>)
						class_method_match = CLASS_METHOD_PATTERN.match(line)
						if class_method_match:
							defined_functions.add(class_method_match.group(1))

						# Detect Logger methods specifically
						logger_method_match = LOGGER_METHOD_PATTERN.search(line)
						if logger_method_match:
							defined_functions.add(logger_method_match.group(1))

						# Detect class definitions
						class_match = re.match(r"^\s*class\s+([a-zA-Z_][a-zA-Z0-9_]*)", line)
						if class_match:
							defined_classes.add(class_match.group(1))

	return defined_functions, defined_classes




def find_function_calls(file_path):
	"""Extract function calls from a C++ file, ignoring comments."""
	function_calls = set()
	in_multiline_comment = False

	with open(file_path, "r", encoding="utf-8", errors="ignore") as file:
		for line in file:
			line = line.strip()

			# Handle multi-line comments
			if in_multiline_comment:
				if MULTI_LINE_COMMENT_END.search(line):
					in_multiline_comment = False
				continue
			if MULTI_LINE_COMMENT_START.search(line):
				in_multiline_comment = True
				continue

			# Skip single-line comments
			if SINGLE_LINE_COMMENT.match(line):
				continue

			# Extract function calls
			matches = FUNCTION_CALL_PATTERN.findall(line)
			for match in matches:
				if "::" in match:
					if match.startswith("std::"):
						continue  # Ignore std:: functions
					function_calls.add(match)
				else:
					function_calls.add(match)

	return function_calls


def analyze_file_structure(file_path):
	"""Check file structure: count lines, function definitions, and detect large functions."""
	function_count = 0
	total_lines = 0
	in_function = False
	function_start_line = 0
	function_name = ""

	warnings = []

	with open(file_path, "r", encoding="utf-8", errors="ignore") as file:
		lines = file.readlines()
		total_lines = len(lines)

		for line_num, line in enumerate(lines, start=1):
			line = line.strip()

			# Skip comments
			if SINGLE_LINE_COMMENT.match(line) or MULTI_LINE_COMMENT_START.search(line):
				continue


			# Detect function end (closing bracket alone in a line)
			if in_function and line == "}":
				function_length = line_num - function_start_line
				if function_length > MAX_FUNCTION_LINES:
					warnings.append(
						f"⚠️ Function `{function_name}` in `{file_path}` has {function_length} lines (limit: {MAX_FUNCTION_LINES})."
					)
				in_function = False

	# File-level warnings
	if total_lines > MAX_FILE_LINES:
		warnings.append(f"⚠️ File `.{file_path}` has {total_lines} lines (limit: {MAX_FILE_LINES}).")

	if function_count > MAX_FILE_FUNCTIONS and not file_path.endswith((".hpp", ".h")):
		warnings.append(f"⚠️ File `.{file_path}` has {function_count} functions (limit: {MAX_FILE_FUNCTIONS}).")


	return warnings


def check_functions(directory):
	"""Scan all `.cpp` and `.hpp` files and check for disallowed function calls."""
	disallowed = {}

	# Get all user-defined functions and classes
	defined_functions, defined_classes = find_defined_functions_and_classes(directory)

	for root, _, files in os.walk(directory):
		for file in files:
			if file.endswith((".cpp", ".hpp")):
				file_path = os.path.abspath(os.path.join(root, file))
				function_calls = find_function_calls(file_path)

				# Find disallowed functions
				for func in function_calls:
					if (
						func.startswith("std::")
						or any(func.startswith(cls + "::") for cls in defined_classes)
						or func in CPP_KEYWORDS
						or func in ALLOWED_FUNCTIONS
						or func in defined_functions
						or func in IGNORED_FUNCTIONS_AND_KEYWORDS
					):
						continue

					if file_path not in disallowed:
						disallowed[file_path] = []
					disallowed[file_path].append(func)

	return disallowed


def main():
	project_dir = "."
	warnings = []

	# Check function calls
	disallowed_functions = check_functions(project_dir)

	# Check file structure
	for root, _, files in os.walk(project_dir):
		for file in files:
			if file.endswith((".cpp", ".hpp")):
				file_path = os.path.abspath(os.path.join(root, file))
				warnings.extend(analyze_file_structure(file_path))

	if warnings:
		print("\n⚠️ **Warnings:**")
		for warning in warnings:
			print(warning)

	if disallowed_functions:
		print("\n❌ **Disallowed Function Calls Found:**")
		for file, funcs in disallowed_functions.items():
			print(f"\n{file}:")
			for func in funcs:
				print(f"  - {func}")

	print("\n✅ **Analysis Complete.**")


if __name__ == "__main__":
	main()
