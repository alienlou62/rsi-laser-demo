#!/bin/bash

# Real-Time Laser Tracking System - C# Runner Script
# This script runs the C# version using .NET 9 app style

echo "==============================================="
echo " Real-Time Laser Tracking System (C#)"
echo "==============================================="
echo

# Check if .NET 9 is installed
if ! command -v dotnet &> /dev/null; then
    echo "Error: .NET is not installed or not in PATH"
    echo "Please install .NET 9 SDK first:"
    echo "  https://dotnet.microsoft.com/download/dotnet/9.0"
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

#!/bin/bash

# .NET 10 App-Style Runner - No Project Files Needed!
# Based on: https://devblogs.microsoft.com/dotnet/announcing-dotnet-run-app/

echo "==============================================="
echo " gRPC Camera Server (.NET 10 App Style)"
echo "==============================================="
echo

# Add .NET to PATH
export PATH="$PATH:/home/rsi/.dotnet"

# Check .NET version
DOTNET_VERSION=$(dotnet --version)
echo "Detected .NET version: $DOTNET_VERSION"

# Change to the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "Running from: $SCRIPT_DIR"
echo

# .NET 10 direct execution - no project files needed!
echo "Running with .NET 10 app-style execution..."
dotnet run LaserTrackingSystem.cs

echo
echo "Application ended."

echo
echo "Application ended."
