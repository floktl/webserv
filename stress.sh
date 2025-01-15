#!/bin/bash

# Configuration
set -e  # Exit on error

# Global variables for process management
declare -A CHILD_PIDS
MONITOR_PID=""
CLEANUP_DONE=0

# Signal handling function
cleanup() {
	local signal=$1
	if [ "$CLEANUP_DONE" -eq 1 ]; then
		return
	fi

	log_message "Received signal $signal - starting clean shutdown..." "$YELLOW"
	CLEANUP_DONE=1

	# Kill resource monitor if running
	if [ ! -z "$MONITOR_PID" ]; then
		kill -TERM "$MONITOR_PID" 2>/dev/null || true
	fi

	# Kill all child processes gracefully
	for pid in "${CHILD_PIDS[@]}"; do
		if kill -0 "$pid" 2>/dev/null; then
			kill -TERM "$pid" 2>/dev/null || true
		fi
	done

	# Wait for children to finish (max 5 seconds)
	local timeout=5
	while [ $timeout -gt 0 ] && [ "$(jobs -p | wc -l)" -gt 0 ]; do
		sleep 1
		((timeout--))
	done

	# Force kill remaining processes
	for pid in "${CHILD_PIDS[@]}"; do
		if kill -0 "$pid" 2>/dev/null; then
			kill -9 "$pid" 2>/dev/null || true
		fi
	done

	log_message "Cleanup complete. Exiting." "$GREEN"
	exit 0
}

# Register signal handlers
trap 'cleanup SIGTERM' SIGTERM
trap 'cleanup SIGINT' SIGINT
trap 'cleanup SIGHUP' SIGHUP
LOG_FILE="server_test_$(date +%Y%m%d_%H%M%S).log"
DURATION=300  # Test duration in seconds
CONCURRENT_USERS=50
RAMP_UP=10    # Number of users to add per interval
INTERVAL=30   # Seconds between ramping up users

# Color definitions
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Expected content types for each endpoint
declare -A EXPECTED_CONTENT=(
	["http://localhost:8080/"]="text/html"
	["http://localhost:8080/add"]="application/json"
	["http://localhost:9090/"]="text/html"
	["http://localhost:9090/api"]="application/json"
	["http://localhost:8003/"]="text/html"
	["http://localhost:8004/"]="application/json"
	["http://localhost:8006/api"]="application/json"
	["http://localhost:8001/"]="text/html"
	["http://localhost:8002/"]="application/json"
)

# Test endpoints
ENDPOINTS=(
	"http://localhost:8080/"
	"http://localhost:8080/add"
	"http://localhost:9090/"
	"http://localhost:9090/api"
	"http://localhost:8003/"
	"http://localhost:8004/"
	"http://localhost:8006/api"
	"http://localhost:8001/"
	"http://localhost:8002/"
)

# Function to log messages with timestamp and color
log_message() {
	local message=$1
	local color=${2:-$NC}
	echo -e "${color}[$(date '+%Y-%m-%d %H:%M:%S')] $message${NC}" | tee -a "$LOG_FILE"
}

# Function to validate response content
validate_response() {
	local endpoint=$1
	local response=$2
	local content_type=$3
	local expected_type=${EXPECTED_CONTENT[$endpoint]}

	if [[ "$content_type" != *"$expected_type"* ]]; then
		log_message "Content type mismatch for $endpoint. Expected: $expected_type, Got: $content_type" "$RED"
		return 1
	fi

	# Validate HTML content
	if [[ "$expected_type" == "text/html" ]]; then
		if ! echo "$response" | grep -q "<!DOCTYPE html>\|<html"; then
			log_message "Invalid HTML response from $endpoint" "$RED"
			return 1
		fi
	fi

	# Validate JSON content
	if [[ "$expected_type" == "application/json" ]]; then
		if ! echo "$response" | jq . >/dev/null 2>&1; then
			log_message "Invalid JSON response from $endpoint" "$RED"
			return 1
		fi
	fi

	return 0
}

# Enhanced server check function
check_server() {
	local endpoint=$1
	local response
	local content_type
	local http_code

	# Capture full response and headers
	response=$(curl -s -i "$endpoint")
	http_code=$(echo "$response" | grep -i "HTTP/" | awk '{print $2}')
	content_type=$(echo "$response" | grep -i "Content-Type:" | awk '{print $2}')
	body=$(echo "$response" | awk 'BEGIN{RS="\r\n\r\n"} NR==2')

	# Color code based on status
	if [ "$http_code" -eq 200 ]; then
		log_message "Server $endpoint is responding (HTTP $http_code)" "$GREEN"
		validate_response "$endpoint" "$body" "$content_type"
		return $?
	elif [ "$http_code" -ge 400 ] && [ "$http_code" -lt 500 ]; then
		log_message "Client error at $endpoint (HTTP $http_code)" "$YELLOW"
		return 1
	elif [ "$http_code" -ge 500 ]; then
		log_message "Server error at $endpoint (HTTP $http_code)" "$RED"
		return 1
	else
		log_message "Unexpected response from $endpoint (HTTP $http_code)" "$RED"
		return 1
	fi
}

# Enhanced resource monitoring function
monitor_resources() {
	while true; do
		local cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}')
		local mem_usage=$(free -m | awk 'NR==2{printf "%.2f%%", $3*100/$2}')
		local load_avg=$(uptime | awk -F'load average:' '{print $2}')

		# Color code based on thresholds
		if (( $(echo "$cpu_usage > 80" | bc -l) )); then
			log_message "CPU Usage: ${cpu_usage}%" "$RED"
		elif (( $(echo "$cpu_usage > 60" | bc -l) )); then
			log_message "CPU Usage: ${cpu_usage}%" "$YELLOW"
		else
			log_message "CPU Usage: ${cpu_usage}%" "$GREEN"
		fi

		if (( $(echo "$mem_usage > 80" | bc -l) )); then
			log_message "Memory Usage: ${mem_usage}" "$RED"
		elif (( $(echo "$mem_usage > 60" | bc -l) )); then
			log_message "Memory Usage: ${mem_usage}" "$YELLOW"
		else
			log_message "Memory Usage: ${mem_usage}" "$GREEN"
		fi

		log_message "Load Average: $load_avg" "$BLUE"
		sleep 10
	done
}

# Enhanced request function with response validation
make_requests() {
	local endpoint=$1
	local id=$2
	local temp_file=$(mktemp)

	while true; do
		for method in "GET" "POST" "DELETE"; do
			# Capture full response including headers
			response=$(curl -s -i -X "$method" "$endpoint")
			http_code=$(echo "$response" | grep -i "HTTP/" | awk '{print $2}')
			content_type=$(echo "$response" | grep -i "Content-Type:" | awk '{print $2}')
			body=$(echo "$response" | awk 'BEGIN{RS="\r\n\r\n"} NR==2')

			# Color code based on status
			if [ "$http_code" -eq 200 ]; then
				color=$GREEN
			elif [ "$http_code" -ge 400 ] && [ "$http_code" -lt 500 ]; then
				color=$YELLOW
			else
				color=$RED
			fi

			log_message "Client $id - $method $endpoint - Response: $http_code" "$color"

			# Validate response content
			echo "$body" > "$temp_file"
			if ! validate_response "$endpoint" "$body" "$content_type"; then
				log_message "Response validation failed for $endpoint" "$RED"
			fi

			# Simulate realistic user behavior with random delays
			sleep $(awk 'BEGIN{print rand()/2}')
		done
	done
	rm -f "$temp_file"
}

# Main test execution
main() {
	log_message "Starting enhanced server load test" "$BLUE"

	# Initial server check
	for endpoint in "${ENDPOINTS[@]}"; do
		check_server "$endpoint" || {
			log_message "Critical: Unable to reach $endpoint. Aborting test." "$RED"
			exit 1
		}
	done

	# Start resource monitoring in background
	monitor_resources &
	MONITOR_PID=$!

	# Gradually increase load
	current_users=0
	while [ $current_users -lt $CONCURRENT_USERS ]; do
		for ((i=0; i<RAMP_UP; i++)); do
			if [ $current_users -ge $CONCURRENT_USERS ]; then
				break
			fi

			endpoint=${ENDPOINTS[$((RANDOM % ${#ENDPOINTS[@]}))]}
			make_requests "$endpoint" "$current_users" &
			CHILD_PIDS[$!]=$!
			current_users=$((current_users + 1))
			log_message "Started client $current_users (PID: $!)" "$BLUE"
		done

		sleep $INTERVAL
	done

	log_message "Reached target load of $CONCURRENT_USERS concurrent users" "$GREEN"
	sleep $DURATION

	# Cleanup
	log_message "Test complete, cleaning up" "$BLUE"
	kill $MONITOR_PID
	pkill -P $$

	log_message "Test finished. Results saved in $LOG_FILE" "$GREEN"
}

# Execute main function
main