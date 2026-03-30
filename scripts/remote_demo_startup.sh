#!/usr/bin/env bash

set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SETTINGS_XML="/etc/laser_demo/settings.xml"
PRIMARY_NIC="enp1s0"
CAMERA_NIC="enp2s0"

POST_KILL_WAIT_SECS=3
POST_CLEANUP_WAIT_SECS=2
RAPIDSERVER_WAIT_SECS=5
CAMERA_WAIT_SECS=5
CAMERA_NIC_RESET_WAIT_SECS=5
CAMERA_NIC_RECOVERY_WAIT_SECS=5
RSICONFIG_WAIT_SECS=25
PRIMARY_NIC_RECOVERY_WAIT_SECS=2

LOG_DIR="$REPO_DIR/run_logs"
PID_DIR="$REPO_DIR/run_pids"

print_step() {
  printf '\n==> %s\n' "$1"
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1"
    exit 1
  fi
}

ensure_dir() {
  mkdir -p "$1"
}

is_pid_running() {
  local pid="$1"
  kill -0 "$pid" >/dev/null 2>&1
}

start_background_process() {
  local name="$1"
  local command="$2"

  local log_file="$LOG_DIR/${name}.log"
  local pid_file="$PID_DIR/${name}.pid"

  print_step "Starting $name"
  echo "Command: $command"
  echo "Log: $log_file"

  rm -f "$pid_file"

  bash -lc "
    cd '$REPO_DIR'
    nohup bash -lc \"$command\" > '$log_file' 2>&1 &
    echo \$! > '$pid_file'
  "

  if [[ ! -f "$pid_file" ]]; then
    echo "Failed to create PID file for $name"
    exit 1
  fi

  local pid
  pid="$(cat "$pid_file")"

  if [[ -z "$pid" ]]; then
    echo "Empty PID for $name"
    exit 1
  fi

  sleep 1

  if ! is_pid_running "$pid"; then
    echo "$name exited immediately. Check log: $log_file"
    exit 1
  fi

  echo "$name started with PID $pid"
}

print_step "Pre-checks"
echo "Repo: $REPO_DIR"
echo "Settings: $SETTINGS_XML"
echo "Primary NIC: $PRIMARY_NIC"
echo "Camera NIC: $CAMERA_NIC"
echo "Log dir: $LOG_DIR"
echo "PID dir: $PID_DIR"

require_cmd sudo
require_cmd pkill
require_cmd rm
require_cmd rsiconfig
require_cmd dotnet
require_cmd ip
require_cmd nohup

ensure_dir "$LOG_DIR"
ensure_dir "$PID_DIR"

if [[ ! -f "$SETTINGS_XML" ]]; then
  echo "Missing settings file: $SETTINGS_XML"
  exit 1
fi

print_step "Configuring RT environment"
sudo /rsi/rt-configure.sh "$PRIMARY_NIC" 3

print_step "Cleaning stale RSI and camera processes"
sudo pkill -9 -f 'rapidserver|rmpnetwork|rttaskmanager|rmp|rsiconfig' || true
pkill -f HttpCameraServer || true
pkill -f 'dotnet run --project HttpCameraServer.csproj' || true
sleep "$POST_KILL_WAIT_SECS"

print_step "Clearing shared memory, sockets, and temp files"
sudo rm -f /dev/shm/RSI.* /dev/shm/sem.RSI.* /tmp/rsi_camera_data.json /tmp/rsi_rt_task_running /tmp/rapidserver-50061.sock

print_step "Resetting writable logs"
sudo rm -f "$REPO_DIR/rttaskapi.log" "$HOME/rttaskapi.log" "$HOME/rttaskmanager.log"
rm -f "$LOG_DIR"/*.log "$PID_DIR"/*.pid 2>/dev/null || true
sleep "$POST_CLEANUP_WAIT_SECS"

print_step "Ensuring primary NIC is up"
sudo ip link set "$PRIMARY_NIC" up
sleep "$PRIMARY_NIC_RECOVERY_WAIT_SECS"

start_background_process "rapidserver" "'$REPO_DIR/scripts/rapidserver_run.sh'"
sleep "$RAPIDSERVER_WAIT_SECS"

start_background_process "camera_server" "'$REPO_DIR/scripts/server_camera_run.sh'"
sleep "$CAMERA_WAIT_SECS"

print_step "Resetting camera NIC"
sudo ip link set "$CAMERA_NIC" down
sleep "$CAMERA_NIC_RESET_WAIT_SECS"
sudo ip link set "$CAMERA_NIC" up
sleep "$CAMERA_NIC_RECOVERY_WAIT_SECS"

start_background_process "rsiconfig" "rsiconfig '$SETTINGS_XML' --cpu-affinity 3 --primary-nic '$PRIMARY_NIC'"
sleep "$RSICONFIG_WAIT_SECS"

print_step "Startup sequence launched"
echo "Processes are running in the background."
echo
echo "Logs:"
echo "  tail -f '$LOG_DIR/rapidserver.log'"
echo "  tail -f '$LOG_DIR/camera_server.log'"
echo "  tail -f '$LOG_DIR/rsiconfig.log'"
echo
echo "PIDs:"
for pid_file in "$PID_DIR"/*.pid; do
  [[ -e "$pid_file" ]] || continue
  printf '  %s: %s\n' "$(basename "$pid_file" .pid)" "$(cat "$pid_file")"
done