while true; do
    cpu_usage=$(ps -p $(pgrep webserv) -o %cpu | tail -n 1)
    mem_usage=$(ps -p $(pgrep webserv) -o %mem | tail -n 1)

    if (( $(echo "$cpu_usage > 50" | bc -l) )) || (( $(echo "$mem_usage > 50" | bc -l) )); then
        pkill webserv
        rm -f /tmp/webserv.*
        echo "Webserver stopped due to high resource usage - CPU: ${cpu_usage}%, Memory: ${mem_usage}%"
        break
    fi
    sleep 1
done &