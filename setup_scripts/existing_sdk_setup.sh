#!/bin/bash
# v0.9
set -e

#SDK path variable: enter the absolute path of your existing SDK 11 here
#e.g. SDK_PATH="C:/ABC/nRF5_SDK_11.0.0_89a8197" (Make sure to use / and not \)
export SDK_PATH="C:/Repos/nRF5_SDK_11.0.0_89a8197"

if [ -n "$SDK_PATH" ]; then
    # Go into cmd to run create_symlinks.bat
    cmd //c create_symlinks.bat

    echo "Setting up crypto libs..."
    bash crypto_setup_all.sh

    echo "SDK set-up Complete! Press Enter to Exit..."
    read
else
    echo "SDK_PATH variable is empty, please edit SDK_PATH inside existing_sdk_setup.sh"
    echo "Press Enter to Exit..."
    read
fi
