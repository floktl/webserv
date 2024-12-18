FROM ubuntu:latest

# Update package lists and install essential packages
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
    valgrind \
    make \
    gcc \
    g++ \
    bash \
    curl \
    php \
    php-cgi \
    python3 && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY ./ /app

CMD ["bash"]