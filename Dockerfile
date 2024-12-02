FROM ubuntu:latest

# Install Valgrind
RUN apt-get update && \
    apt-get install -y valgrind && \
    apt-get clean

# Set up environment variables (optional)
ENV DEBIAN_FRONTEND=noninteractive

# Open the necessary ports
# Default to making ports available, for instance, port 8080.
# This line is a placeholder for your actual service configuration.
EXPOSE 8080

# Map all ports as needed. To achieve dynamic forwarding between host and container:
# Use docker run with the -P flag to publish all exposed ports to random ports on the host.
# Example: docker run -P my_container

# Use -p 8080:8080 during 'docker run' to map port 8080 from host to container as per your requirement.

# Define volume to mount current directory to /app
VOLUME ["/app"]

# Start with a base command (can be replaced by your own application command)
CMD ["/bin/bash"]
