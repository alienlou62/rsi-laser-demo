#!/usr/bin/env bash

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$BASE_DIR/pids"

stop_job() {
  local name="$1"
  local pid_file="$PID_DIR/${name}.pid"

  if [[ ! -f "$pid_file" ]]; then
    echo "$name: no PID file"
    return
  fi

  local pid
  pid="$(cat "$pid_file")"

  if kill -0 "$pid" >/dev/null 2>&1; then
    echo "Stopping $name (PID $pid)"
    kill "$pid"
    sleep 1

    if kill -0 "$pid" >/dev/null 2>&1; then
      echo "  forcing kill"
      kill -9 "$pid"
    fi
  else
    echo "$name: not running"
  fi

  rm -f "$pid_file"
}

stop_job "stress_full"
stop_job "stress_cpu"
stop_job "iperf"

echo "Done."