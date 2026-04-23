#!/bin/bash
# Install dependencies for GitHub Actions Ubuntu Runner
# This ensures CodeQL can successfully compile the project during its analysis phase.

set -e

echo "Updating apt repositories..."
sudo apt-get update -y

echo "Installing core dependencies (Clang, libbpf)..."
sudo apt-get install -y clang libbpf-dev pkg-config

echo "Installing optional GTK4/Libadwaita dependencies for GUI compilation..."
sudo apt-get install -y libgtk-4-dev libadwaita-1-dev

echo "Dependencies installed successfully."
