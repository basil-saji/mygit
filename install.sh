#!/bin/bash

echo "Installing mygit..."

# Stop if error
set -e

# Install dependencies (Ubuntu/Debian)
if command -v apt >/dev/null 2>&1; then
    echo "Installing dependencies..."
    sudo apt update
    sudo apt install -y build-essential libssl-dev
fi

# Create temp directory
TMP_DIR=$(mktemp -d)
cd $TMP_DIR

# Download source files
echo "Downloading source..."

curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/main.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/add.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/commit.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/hash.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/init.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/object.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/utils.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/tree.c
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/audit.c

# Download headers
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/add.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/commit.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/hash.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/init.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/object.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/utils.h
curl -sLO https://raw.githubusercontent.com/basil-saji/mygit/main/tree.h

# Compile
echo "Compiling..."
gcc *.c -lssl -lcrypto -o mygit

# Install globally
echo "Installing to /usr/local/bin..."
sudo mv mygit /usr/local/bin/

echo "Installation complete! Run mygit to get started"