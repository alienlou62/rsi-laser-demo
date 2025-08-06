#!/bin/bash

# This script publishes and runs the RapidLaser.Desktop project.
PUBLISH_DIR="$(dirname "$0")/../temp/RapidLaser.Desktop"
PROJECT_PATH="$(dirname "$0")/../ui/RapidLaser.Desktop/RapidLaser.Desktop.csproj"
EXECUTABLE="$PUBLISH_DIR/RapidLaser.Desktop.exe"

# Publish as single file if executable does not exist
if [ ! -f "$EXECUTABLE" ]; then
    echo "Publishing RapidLaser.Desktop as a single executable to $PUBLISH_DIR..."
    dotnet publish "$PROJECT_PATH" -c Release -o "$PUBLISH_DIR" -p:PublishSingleFile=true -p:SelfContained=true
fi

# Run the executable
echo "Running RapidLaser.Desktop..."
"$EXECUTABLE"