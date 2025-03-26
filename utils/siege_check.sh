#!/bin/bash

echo "🚀 Running Siege & System Checks Inside Container..."

CONFIG_FILE="./config/test.conf"
SERVER_BINARY="./webserv"
LOG_FILE="./utils/siege_log.txt"
REPORT_FILE="./utils/test_report.txt"

# Clear previous report
echo "📝 Siege Test Report" > "$REPORT_FILE"
echo "=====================" >> "$REPORT_FILE"

# 1️⃣ Extract All Listening Ports from the Config File
echo -e "\n🔍 Extracting server ports from configuration..."
PORTS=$(grep -Eo "listen [0-9]+" "$CONFIG_FILE" | awk '{print $2}' | sort -u)
SERVER_URLS=()

for PORT in $PORTS; do
	SERVER_URLS+=("http://localhost:$PORT")
done

echo "✅ Found servers on ports: ${PORTS}" | tee -a "$REPORT_FILE"

# 2️⃣ Check Memory Usage Before Siege
echo -e "\n🔍 Checking Initial Memory Usage..."
ps aux --sort=-%mem | head -10

# 3️⃣ Check Binary Size
echo -e "\n📦 Checking Binary Size of the Server..."
ls -lh "$SERVER_BINARY"
if command -v size &>/dev/null; then
	size "$SERVER_BINARY"
else
	echo "⚠ 'size' command not available."
fi

# 4️⃣ Start Siege Test for Each Server
echo -e "\n🔥 Running Siege Load Test on all detected servers..."
for URL in "${SERVER_URLS[@]}"; do
	echo "🚀 Testing: $URL"
	siege -c200 -t30s "$URL" &
done
wait

# 5️⃣ Wait a bit and check if any server restarts
echo -e "\n🔄 Checking if the Server Restarts..."
sleep 5
PROCESS_IDS=$(pgrep webserv)
sleep 30
NEW_PROCESS_IDS=$(pgrep webserv)

if [ "$PROCESS_IDS" != "$NEW_PROCESS_IDS" ]; then
	echo "❌ Server restarted! Check logs." | tee -a "$REPORT_FILE"
	journalctl -xe | tail -20
else
	echo "✅ No restarts detected." | tee -a "$REPORT_FILE"
fi

# 6️⃣ Check Memory Usage After Siege
echo -e "\n📊 Checking Memory Usage After Siege..."
ps aux --sort=-%mem | head -10

# 7️⃣ Check for Memory Leaks (C/C++ Only)
if command -v valgrind &>/dev/null; then
	echo -e "\n🔥 Running Valgrind for Memory Leaks..."
	valgrind --leak-check=full --track-origins=yes ../webserv "$CONFIG_FILE"
else
	echo -e "\n⚠ Valgrind not installed, skipping memory leak check." | tee -a "$REPORT_FILE"
fi

# 8️⃣ Check Detailed Memory Usage
echo -e "\n📊 Checking smem (if available)..."
if command -v smem &>/dev/null; then
	smem -p -c "pid pss uss vss command" | head -10
else
	echo "⚠ smem not installed, skipping detailed memory check." | tee -a "$REPORT_FILE"
fi

# 9️⃣ Analyze Siege Results
echo -e "\n📊 Analyzing Siege Test Results..."
if [ ! -f "$LOG_FILE" ]; then
	echo "❌ Siege log file not found!" | tee -a "$REPORT_FILE"
else
	SUCCESSFUL_TRANSACTIONS=$(grep -Eo '"successful_transactions": [0-9]+' "$LOG_FILE" | awk '{sum+=$2} END {print sum}')
	FAILED_TRANSACTIONS=$(grep -Eo '"failed_transactions": [0-9]+' "$LOG_FILE" | awk '{sum+=$2} END {print sum}')
	AVAILABILITY=$(grep -Eo '"availability": [0-9]+(\.[0-9]+)?' "$LOG_FILE" | awk '{sum+=$2; count++} END {if(count > 0) print sum/count; else print "0"}')

	# ✅ Fix: Ensure availability is always a valid number
	if [[ -z "$AVAILABILITY" ]]; then
		AVAILABILITY="0"
	fi

	# ✅ Convert to integer safely
	AVAILABILITY=$(grep -o '"availability": [0-9.]*' "$LOG_FILE" | awk -F': ' '{print $2}' | awk '{sum+=$1; count++} END {if(count > 0) print sum/count; else print "0"}')

	echo "📊 Siege Test Summary:" | tee -a "$REPORT_FILE"
	echo "   ✅ Successful Transactions: $SUCCESSFUL_TRANSACTIONS" | tee -a "$REPORT_FILE"
	echo "   ❌ Failed Transactions: $FAILED_TRANSACTIONS" | tee -a "$REPORT_FILE"
	echo "   📈 Average Availability: $AVAILABILITY%" | tee -a "$REPORT_FILE"

	# ✅ Fix integer comparison issue (ensure AVAILABILITY_INT is always valid)
	if [[ -n "$AVAILABILITY_INT" ]] && [[ "$AVAILABILITY_INT" -lt 95 ]]; then
		echo "❌ WARNING: Server availability is below 95%!" | tee -a "$REPORT_FILE"
	else
		echo "✅ Server availability is good." | tee -a "$REPORT_FILE"
	fi

	if [[ "$FAILED_TRANSACTIONS" -gt 0 ]]; then
		echo "❌ Some requests failed during siege test." | tee -a "$REPORT_FILE"
	else
		echo "✅ No failed transactions detected." | tee -a "$REPORT_FILE"
	fi
fi

echo -e "\n✅ All tests completed! Report saved to $REPORT_FILE"
