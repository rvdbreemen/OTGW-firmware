#!/usr/bin/env bash
# Setup script for WSL build environment
set -e

echo "============================================================"
echo " Setting up WSL environment for OTGW-firmware build"
echo "============================================================"
echo

# Install required packages
echo "[STEP] Installing required packages..."
sudo apt-get update
sudo apt-get install -y python3 python3-pip python3-venv unzip curl git

# Clean up any Windows-installed arduino packages
echo "[STEP] Cleaning up Windows toolchain..."
rm -rf arduino/packages
rm -rf arduino/staging
rm -rf .tmp

echo
echo "============================================================"
echo " WSL environment setup complete!"
echo " Now run: ./build.sh"
echo "============================================================"
