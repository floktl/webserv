FROM ubuntu:latest

RUN apt-get update && \
	apt-get install -y valgrind make gcc g++ bash && \
	apt-get clean

WORKDIR /app

COPY ./ /app

CMD ["bash"]