#!/bin/bash
# v0.9
# set -e

# SDK version variable
export SDK_VERS=nRF5_SDK_11.0.0_89a8197
export DL_LOCATION=../..

echo "Downloading SDK..."
curl -o $DL_LOCATION/$SDK_VERS.zip http://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v11.x.x/$SDK_VERS.zip

err_code=$?
if [ "$err_code" != "0" ]
then
    echo "Could not download SDK. Press Enter to Exit..."
    read
    exit
fi

echo "Unzipping SDK..."
unzip  $DL_LOCATION/$SDK_VERS.zip -d $DL_LOCATION/$SDK_VERS

err_code=$?
if [ "$err_code" = "50" ]
then
    #Ignore this "disk full" error
    echo "Ignoring Disk Full error"
elif [ "$err_code" != "0" ]
then
    echo "Could not unzip SDK. Does $DL_LOCATION/$SDK_VERS already exist? Press Enter to Exit..."
    read
    exit
fi

# Go into cmd to run create_symlinks.bat
cmd //c create_symlinks.bat

echo "Setting up crypto libs..."
bash crypto_setup_all.sh

echo "Clean up: Removing zip file..."
rm $DL_LOCATION/$SDK_VERS.zip

err_code=$?
if [ "$err_code" != "0" ]
then
    echo "Could not remove SDK zip file. Press Enter to Exit..."
    read
    exit
fi

echo "SDK set-up Complete! Press Enter to Exit..."

read
