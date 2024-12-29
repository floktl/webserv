FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install required packages
RUN apt-get update && apt-get install -y software-properties-common && add-apt-repository universe && apt-get update && apt-get install -y valgrind make gcc g++ bash curl php php-cgi python3 && apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY ./ /app

<<<<<<< HEAD
CMD ["bash"]
=======
CMD ["bash"]
>>>>>>> c627d561f7dffc7f10d43c042ffb173efb2ed733
