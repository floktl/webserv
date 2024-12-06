FROM ubuntu:latest

RUN apt-get update && \
	apt-get install -y valgrind make gcc g++ bash curl php php-cgi python3 && \
	apt-get clean

WORKDIR /app

COPY ./ /app

CMD ["bash"]
