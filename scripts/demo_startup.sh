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

print_step() {
  printf '\n==> %s\n' "$1"
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "Missing required command: $1"
    exit 1
  fi
}

pick_terminal() {
  if command -v gnome-terminal >/dev/null 2>&1; then
    echo "gnome-terminal"
    return
  fi

  if command -v x-terminal-emulator >/dev/null 2>&1; then
    echo "x-terminal-emulator"
    return
  fi

  echo ""
}

launch_terminal() {
  local title="$1"
  local command="$2"
  local terminal_bin="$3"

  case "$terminal_bin" in
    gnome-terminal)
      gnome-terminal --title="$title" -- bash -lc "cd '$REPO_DIR'; $command; printf '\n[%s exited] Press Enter to close... ' '$title'; read -r"
      ;;
    x-terminal-emulator)
      x-terminal-emulator -T "$title" -e bash -lc "cd '$REPO_DIR'; $command; printf '\n[%s exited] Press Enter to close... ' '$title'; read -r"
      ;;
    *)
      echo "No supported terminal emulator found."
      echo "Run these commands manually in separate terminals:"
      echo "  $command"
      exit 1
      ;;
  esac
}

print_step "Pre-checks"
echo "Repo: $REPO_DIR"
echo "Settings: $SETTINGS_XML"
echo "Primary NIC: $PRIMARY_NIC"
echo "Camera NIC: $CAMERA_NIC"

require_cmd sudo
require_cmd pkill
require_cmd rm
require_cmd rsiconfig
require_cmd dotnet
require_cmd ip

TERMINAL_BIN="$(pick_terminal)"
if [[ -z "$TERMINAL_BIN" ]]; then
  echo "Could not find gnome-terminal or x-terminal-emulator."
  exit 1
fi

if [[ ! -f "$SETTINGS_XML" ]]; then
  echo "Missing settings file: $SETTINGS_XML"
  exit 1
fi

print_step "Cleaning stale RSI and camera processes"
sudo pkill -9 -f '/rsi/rapidserver|/rsi/rmpnetwork|/rsi/rttaskmanager|/rsi/rmp|rsiconfig' || true
pkill -f HttpCameraServer || true
pkill -f 'dotnet run --project HttpCameraServer.csproj' || true
sleep "$POST_KILL_WAIT_SECS"

print_step "Clearing shared memory, sockets, and temp files"
sudo rm -f /dev/shm/RSI.* /dev/shm/sem.RSI.* /tmp/rsi_camera_data.json /tmp/rsi_rt_task_running /tmp/rapidserver-50061.sock

print_step "Resetting writable logs"
sudo rm -f "$REPO_DIR/rttaskapi.log" "$HOME/rttaskapi.log" "$HOME/rttaskmanager.log"
sleep "$POST_CLEANUP_WAIT_SECS"

print_step "Launching rapidserver"
launch_terminal "RapidServer" "'$REPO_DIR/scripts/rapidserver_run.sh'" "$TERMINAL_BIN"
sleep "$RAPIDSERVER_WAIT_SECS"

print_step "Launching HTTP camera server"
launch_terminal "Camera Server" "'$REPO_DIR/scripts/server_camera_run.sh'" "$TERMINAL_BIN"
sleep "$CAMERA_WAIT_SECS"

print_step "Resetting camera NIC"
sudo ip link set "$CAMERA_NIC" down
sleep "$CAMERA_NIC_RESET_WAIT_SECS"
sudo ip link set "$CAMERA_NIC" up
sleep "$CAMERA_NIC_RECOVERY_WAIT_SECS"

print_step "Launching rsiconfig"
launch_terminal "RSIConfig" "rsiconfig '$SETTINGS_XML' --cpu-affinity 3 --primary-nic '$PRIMARY_NIC'" "$TERMINAL_BIN"
sleep "$RSICONFIG_WAIT_SECS"

print_step "Launching UI"
launch_terminal "RapidLaser UI" "dotnet run --project '$REPO_DIR/ui/RapidLaser.Desktop/RapidLaser.Desktop.csproj'" "$TERMINAL_BIN"

print_step "Startup sequence launched"
echo "If something fails, check the dedicated terminal window for that process."
