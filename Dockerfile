FROM ubuntu:latest AS builder

RUN apt-get update && \
    apt-get install -q -y \
            autoconf \
            git \
            gcc \
            libssl-dev \
            make \
            wget

COPY . /radmind/
COPY ./.git /radmind/.git/
COPY ./.gitmodules /radmind/

RUN cd radmind && \
	if [ -e radmind ]; then rm -r radmind; fi && \
    sh bootstrap.sh && \
 	wget -O config.sub 'https://git.savannah.gnu.org/cgit/config.git/plain/config.sub' && \
 	wget -O config.guess 'https://git.savannah.gnu.org/cgit/config.git/plain/config.guess' && \
 	cp config.guess libsnet && \
 	cp config.sub libsnet && \
 	autoconf && \
 	cd libsnet && \
 	autoconf && \
 	cd .. && \
 	./configure && \
 	make && \
 	make install && \
 	mkdir -p /var/radmind/{cert,client,postapply,preapply}

# Build rsyslog so we can grab omstdout.so

RUN wget https://www.rsyslog.com/files/download/rsyslog/rsyslog-8.2112.0.tar.gz && \
	tar xzf rsyslog-8.2112.0.tar.gz && \
	cd rsyslog-8.2112.0 && \
	ln -snf /usr/share/zoneinfo/Europe/London /etc/localtime && echo Europe/London > /etc/timezone && \
	apt install -q -y \
		libcurl4-gnutls-dev \
		libestr-dev \
		libfastjson-dev \
		libgcrypt-dev \
		pkg-config \
		uuid-dev \
		zlib1g-dev && \
	./configure --prefix=/usr --enable-omstdout && \
	make && \
	make install

FROM ubuntu:latest

RUN apt-get update && \
    apt-get install -y \
    	libssl-dev

# Add rsyslog (because radmind doesn't log to stdout but rsyslog can w/ omstdout)

COPY ./docker/etc /etc/
COPY --from=builder /usr/lib/rsyslog/omstdout.so /usr/local/lib/omstdout.so
RUN apt-get install -y rsyslog && \
	mv /usr/local/lib/omstdout.so /lib/*-linux-gnu/rsyslog/ && \
	rm /etc/rsyslog.d/50-default.conf


# Copy radmind

COPY --from=builder /usr/local/ /usr/local/
COPY docker/entrypoint /usr/local/bin/entrypoint

ENTRYPOINT ["/usr/local/bin/entrypoint"]
