#!/bin/bash

# Simple Camera Server - .NET 10 App Style Runner
# Runs SimpleCameraServer.cs using .NET 10 file-based execution

echo "==============================================="
echo " Simple Camera Server (.NET 10 App Style)"
echo "==============================================="
echo

# Check if .NET is installed
if ! command -v dotnet &> /dev/null; then
    echo "Error: .NET is not installed or not in PATH"
    echo "Please install .NET 10 SDK first"
    exit 1
fi

# Check .NET version
DOTNET_VERSION=$(dotnet --version)
echo "Detected .NET version: $DOTNET_VERSION"

# Change to the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Running from: $SCRIPT_DIR"
echo

# Run HttpCameraServer.cs with .NET 10 app-style execution
echo "Starting HTTP Camera Server..."
dotnet run ../src/csharp/HttpCameraServer.cs

echo
echo "Camera server ended."
