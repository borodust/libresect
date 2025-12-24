FROM debian:trixie-slim

ARG BUILDER_UID=''
ARG BUILDER_GID=''

ENV LIBRESECT_SRC_DIR=/opt/libresect-src BUILDER_UID=${BUILDER_UID:-1000} BUILDER_GID=${BUILDER_GID:-1000}

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
    ca-certificates lsb-release zlib1g-dev gpg make cmake ninja-build curl libedit-dev libzstd-dev libcurl4-openssl-dev \
    && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN curl -fsSL https://apt.llvm.org/llvm.sh | bash -s -- 21 all

RUN groupadd -g "${BUILDER_GID}" -o libresect \
    && useradd -m -u "${BUILDER_UID}" -g "${BUILDER_GID}" -o -s /bin/bash libresect

COPY entrypoint.sh /usr/local/bin/entrypoint.sh

USER libresect:libresect

ENTRYPOINT ["entrypoint.sh"]