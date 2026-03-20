#!/usr/bin/env bash

set -euo pipefail

SOCKET_PATH="/tmp/rapidserver-50061.sock"

if [[ -S "${SOCKET_PATH}" ]]; then
  if [[ -O "${SOCKET_PATH}" ]]; then
    rm -f "${SOCKET_PATH}"
  else
    owner="$(stat -c '%U:%G' "${SOCKET_PATH}")"
    echo "Cannot start rapidserver: stale socket ${SOCKET_PATH} is owned by ${owner}."
    echo "Run: sudo rm -f ${SOCKET_PATH}"
    echo "After that, launch rapidserver without sudo."
    exit 1
  fi
fi

exec rapidserver "$@"
