#!/bin/bash

echo "🚀 Running Siege & System Checks Inside Container..."

CONFIG_FILE="./config/test.conf"  # Adjust if necessary

# 1️⃣ Extract All Listening Ports from the Config File
echo -e "\n🔍 Extracting server ports from configuration..."
PORTS=$(grep -Eo "listen [0-9]+" "$CONFIG_FILE" | awk '{print $2}' | sort -u)
SERVER_URLS=()

for PORT in $PORTS; do
    SERVER_URLS+=("http://localhost:$PORT")
done

echo "✅ Found servers on ports: ${PORTS}"

# 2️⃣ Check Memory Usage Before Siege
echo -e "\n🔍 Checking Initial Memory Usage..."
ps aux --sort=-%mem | head -10

# 3️⃣ Check Binary Size
echo -e "\n📦 Checking Binary Size of the Server..."
ls -lh ./webserv
size ./webserv || echo "⚠ 'size' command not available."

# 4️⃣ Start Siege Test for Each Server
echo -e "\n🔥 Running Siege Load Test on all detected servers..."
for URL in "${SERVER_URLS[@]}"; do
    echo "🚀 Testing: $URL"
    siege -c50 -t30s "$URL" &
done
wait

# 5️⃣ Wait a bit and check if any server restarts
echo -e "\n🔄 Checking if the Server Restarts..."
sleep 5
PROCESS_IDS=$(pgrep webserv)
sleep 30
NEW_PROCESS_IDS=$(pgrep webserv)

if [ "$PROCESS_IDS" != "$NEW_PROCESS_IDS" ]; then
    echo "❌ Server restarted! Check logs."
    journalctl -xe | tail -20
else
    echo "✅ No restarts detected."
fi

# 6️⃣ Check Memory Usage After Siege
echo -e "\n📊 Checking Memory Usage After Siege..."
ps aux --sort=-%mem | head -10

# 7️⃣ Check for Memory Leaks (C/C++ Only)
if command -v valgrind &>/dev/null; then
    echo -e "\n🔥 Running Valgrind for Memory Leaks..."
    valgrind --leak-check=full --track-origins=yes ./webserv "$CONFIG_FILE"
else
    echo -e "\n⚠ Valgrind not installed, skipping memory leak check."
fi

# 8️⃣ Check Detailed Memory Usage
echo -e "\n📊 Checking smem (if available)..."
if command -v smem &>/dev/null; then
    smem -p -c "pid pss uss vss command" | head -10
else
    echo "⚠ smem not installed, skipping detailed memory check."
fi

echo -e "\n✅ All tests completed!"
