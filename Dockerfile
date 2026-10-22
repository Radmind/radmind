ARG UBUNTU_VERSION=22.04

FROM ubuntu:${UBUNTU_VERSION} AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG RSYSLOG_VERSION=8.2112.0
ARG RSYSLOG_SHA256=6a2a973861e9266db37bd2b7b9f672b6b970bfcd743a397b8eee6b0dc4852c41

RUN apt-get update && \
	apt-get install -y --no-install-recommends \
		autoconf \
		autotools-dev \
		ca-certificates \
		gcc \
		libcurl4-gnutls-dev \
		libestr-dev \
		libfastjson-dev \
		libgcrypt-dev \
		libssl-dev \
		make \
		pkg-config \
		uuid-dev \
		wget \
		zlib1g-dev && \
	rm -rf /var/lib/apt/lists/*

WORKDIR /radmind
COPY . .

RUN test -f libsnet/configure.ac && \
	(cd libsnet && autoconf) && \
	autoconf && \
	cp /usr/share/misc/config.sub . && \
	cp /usr/share/misc/config.guess . && \
	cp config.sub config.guess libsnet/ && \
	./configure && \
	make clean && \
	make -j"$(nproc)" && \
	make install

# Build the output module that forwards radmind's syslog messages to stdout.
RUN wget --https-only --tries=3 \
		-O /tmp/rsyslog.tar.gz \
		"https://www.rsyslog.com/files/download/rsyslog/rsyslog-${RSYSLOG_VERSION}.tar.gz" && \
	echo "${RSYSLOG_SHA256}  /tmp/rsyslog.tar.gz" | sha256sum -c - && \
	mkdir /tmp/rsyslog && \
	tar -xzf /tmp/rsyslog.tar.gz --strip-components=1 -C /tmp/rsyslog && \
	cd /tmp/rsyslog && \
	./configure --prefix=/usr --enable-omstdout && \
	make -j"$(nproc)" && \
	make install

FROM ubuntu:${UBUNTU_VERSION}

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
	apt-get install -y --no-install-recommends \
		libssl3 \
		rsyslog && \
	rm -rf /var/lib/apt/lists/* && \
	rm -f /etc/rsyslog.d/50-default.conf

# Add rsyslog because radmind logs through syslog rather than stdout.
COPY docker/etc/ /etc/
COPY --from=builder /usr/lib/rsyslog/omstdout.so /tmp/omstdout.so
RUN module_dir="$(find /usr/lib -type d -name rsyslog -print -quit)" && \
	test -n "${module_dir}" && \
	install -m 0755 /tmp/omstdout.so "${module_dir}/omstdout.so" && \
	rm /tmp/omstdout.so

COPY --from=builder /usr/local/ /usr/local/
COPY docker/entrypoint /usr/local/bin/entrypoint

EXPOSE 6222

ENTRYPOINT ["/usr/local/bin/entrypoint"]
