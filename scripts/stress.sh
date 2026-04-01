#!/usr/bin/env bash

set -euo pipefail

BASE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PID_DIR="$BASE_DIR/pids"
LOG_DIR="$BASE_DIR/logs"

mkdir -p "$PID_DIR" "$LOG_DIR"

echo "Starting jobs (detached)..."

start_job() {
  local name="$1"
  local cmd="$2"

  local log="$LOG_DIR/${name}.log"
  local pid_file="$PID_DIR/${name}.pid"

  rm -f "$pid_file"

  echo "Starting $name"
  echo "  log: $log"

  nohup bash -c "$cmd" > "$log" 2>&1 &

  local pid=$!
  echo "$pid" > "$pid_file"

  echo "  pid: $pid"
}

# Job 1
start_job "stress_full" "/usr/bin/timeout 45m /usr/bin/stress-ng \
  --switch 8 \
  --matrix 8 \
  --timer 8 \
  --netdev 8 \
  --vm 8 \
  --vm-bytes 90% \
  --cache 4 \
  --cache-level 3 \
  --cache-enable-all \
  --cache-no-affinity \
  --oom-avoid \
  -t 45m"

# Job 2
start_job "stress_cpu" "/usr/bin/timeout 5m /usr/bin/stress-ng \
  --cpu 2 \
  --cpu-load 98 \
  --sched fifo \
  --sched-prio 10 \
  --oom-avoid \
  -t 5m"

# Job 3
start_job "iperf" "/usr/bin/iperf3 -c 192.168.111.98 \
  --bidir \
  -P 4 \
  -t 3300"

echo
echo "All jobs started."
echo "Logs: $LOG_DIR"
echo "PIDs: $PID_DIR"
echo "Monitor: tail -f $LOG_DIR/*.log"