#!/bin/bash

# HTTP Camera Server Runner
# Starts a lightweight HTTP server that serves camera frame data to UI clients
# Listens on http://localhost:50080/ with endpoints:
#   - GET /camera/frame - Returns latest camera frame data as JSON
#   - GET /status - Returns server health status
# Uses .NET 10 file-based execution (no project files needed)


## METHODS
printTitleBox() {
  local title="$1"
  local padding=8

  # Colors (blue for both border & text)
  local RESET=$'\033[0m'
  local COLOR=$'\033[38;5;39m'   # blue

  # Width calculations
  local inner=$(( ${#title} + padding * 2 ))

  # Build box lines
  local top="${COLOR}╭$(printf '─%.0s' $(seq 1 $inner))╮${RESET}"
  local mid="${COLOR}│$(printf ' %.0s' $(seq 1 $padding))${title}$(printf ' %.0s' $(seq 1 $padding))│${RESET}"
  local bot="${COLOR}╰$(printf '─%.0s' $(seq 1 $inner))╯${RESET}"

  # Print
  echo "$top"
  echo "$mid"
  echo "$bot"
}


## APP
# Print title
printTitleBox "Http Camera Server (.NET 10 app-style)"
echo

# Check if .NET is installed
if ! command -v dotnet &> /dev/null; then
    echo "ERROR: .NET is not installed or not in PATH. Please install .NET 10 SDK."
    exit 1
fi

# Check if .NET 10 SDK is available
echo "Checking for .NET 10 SDK..."
if ! dotnet --list-sdks | grep -q "^10\."; then
    echo "ERROR: .NET 10 SDK is not installed."
    echo "Available SDKs:"
    dotnet --list-sdks
    echo "Please install .NET 10 SDK."
    exit 1
fi

echo ".NET 10 SDK found."
echo

# Change to the camera server directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CAMERA_DIR="$(cd "$SCRIPT_DIR/../servers/camera" && pwd)"
cd "$CAMERA_DIR"

# Run HttpCameraServer.cs with .NET 10 app-style execution
echo "🟢 Starting HTTP Camera Server (dotnet run HttpCameraServer.cs)..."
echo

dotnet run HttpCameraServer.cs

echo
echo "🔴 Camera server ended."


