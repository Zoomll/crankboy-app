#!/bin/bash

# --- Configuration ---
PDX_DIR="./CrankBoy.pdx"
ZIP_FILE="${PDX_DIR%.pdx}.zip"

# --- Helper Function ---
# A function that prints a command and then executes it.
# This gives us a clean, manual trace of the script's actions.
run() {
    # Print the command in a clean, readable format.
    echo "$@"
    # Execute the command, passing all arguments exactly as received.
    "$@"
}

# --- Script Execution ---
# Exit immediately if a command exits with a non-zero status.
set -e

# --- Cleanup ---
echo "--- Cleaning up old artifacts ---"
if [ -d "$PDX_DIR" ]; then
    run rm -r "$PDX_DIR"
fi
if [ -f "$ZIP_FILE" ]; then
    run rm "$ZIP_FILE"
fi

# --- Build Step ---
echo "--- Building project for device ---"
run make clean
run make device

# --- Archiving Step ---
echo "--- Archiving the .pdx directory ---"

if command -v zip >/dev/null 2>&1; then
    run echo "Found 'zip'. Using standard zip command."
    run zip -r -q "$ZIP_FILE" "$PDX_DIR"
else
    run echo "Warning: 'zip' command not found. Looking for '7z'."
    if command -v 7z >/dev/null 2>&1; then
        run echo "Found '7z'. Using 7z to create zip-compatible archive."
        run 7z a -tzip "$ZIP_FILE" "$PDX_DIR"
    else
        echo "Error: Neither 'zip' nor '7z' command found." >&2
        echo "Please install one of them to create the archive." >&2
        exit 1
    fi
fi

echo ""
echo "✅ Successfully created release archive: $ZIP_FILE"
