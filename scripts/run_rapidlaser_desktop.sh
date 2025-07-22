#!/bin/bash

# to run:
# 1. make it executable -> chmod +x run_rapidlaser_desktop.sh
# 2. run it             -> ./run_rapidlaser_desktop.sh

# This script runs the RapidLaser.Desktop project using dotnet.
dotnet run --project ../ui/RapidLaser.Desktop/RapidLaser.Desktop.csproj
