echo "Creating symlinks"

::if SDK_PATH is undefined, then download_sdk_setup.sh was mostly likely executed
::If it is defined, then existing_sdk_setup.sh was most likely executed

IF "%SDK_PATH%"=="" GOTO sdk_downloaded

:sdk_existing
mklink /J "../sdk_components" "%SDK_PATH%/components"
mklink /J "../sdk_external" "%SDK_PATH%/external"
goto:eof

:sdk_downloaded
mklink /J "../sdk_components" "%DL_LOCATION%/%SDK_VERS%/components"
mklink /J "../sdk_external" "%DL_LOCATION%/%SDK_VERS%/external"
goto:eof
