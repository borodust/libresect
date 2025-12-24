#!/usr/bin/env bash
set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

BUILDER_UID=$(id -u)
BUILDER_GID=$(id -g)

echo "Using UID $BUILDER_UID and GID $BUILDER_GID"
docker build --build-arg "BUILDER_UID=$BUILDER_UID" --build-arg "BUILDER_GID=$BUILDER_GID" -t libresect-builder:linux "$SCRIPT_DIR/linux"

docker run --rm -v "$SCRIPT_DIR/..:/opt/libresect-src" libresect-builder:linux