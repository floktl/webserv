FROM ubuntu:22.04

# Avoid prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install required packages
RUN apt-get update && \
	apt-get install -y \
	software-properties-common && \
	add-apt-repository universe && \
	apt-get update && \
	apt-get install -y \
	valgrind \
	make \
	siege \
	gcc \
	g++ \
	bash \
	curl \
	php \
	php-cgi \
	python3 \
	nano && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/*

# Find PHP configuration directory and add our settings
RUN PHP_INI_DIR=$(php --ini | grep "Loaded Configuration File" | sed -e "s|.*:\s*||") && \
	echo "Creating custom PHP config in directory: $(dirname $PHP_INI_DIR)" && \
	echo "upload_max_filesize = 5G" > $(dirname $PHP_INI_DIR)/conf.d/99-upload-limits.ini && \
	echo "post_max_size = 5G" >> $(dirname $PHP_INI_DIR)/conf.d/99-upload-limits.ini && \
	echo "memory_limit = 6G" >> $(dirname $PHP_INI_DIR)/conf.d/99-upload-limits.ini && \
	echo "max_execution_time = 3600" >> $(dirname $PHP_INI_DIR)/conf.d/99-upload-limits.ini && \
	echo "max_input_time = 3600" >> $(dirname $PHP_INI_DIR)/conf.d/99-upload-limits.ini && \
	# Also create the same file for php-cgi
	PHP_CGI_DIR=$(dirname $PHP_INI_DIR | sed 's/cli/cgi/') && \
	mkdir -p $PHP_CGI_DIR/conf.d/ && \
	echo "Creating php-cgi config in: $PHP_CGI_DIR/conf.d/" && \
	echo "upload_max_filesize = 5G" > $PHP_CGI_DIR/conf.d/99-upload-limits.ini && \
	echo "post_max_size = 5G" >> $PHP_CGI_DIR/conf.d/99-upload-limits.ini && \
	echo "memory_limit = 6G" >> $PHP_CGI_DIR/conf.d/99-upload-limits.ini && \
	echo "max_execution_time = 3600" >> $PHP_CGI_DIR/conf.d/99-upload-limits.ini && \
	echo "max_input_time = 3600" >> $PHP_CGI_DIR/conf.d/99-upload-limits.ini

WORKDIR /app

COPY ../ /app

# Customize bash prompt
RUN echo 'PS1="\[\033[34m\]webcontainer: \w# \[\033[0m\]"' >> /etc/bash.bashrc

CMD ["bash"]