# nRF5 SDK for Eddystone™

This is an example implementation of the Eddystone GATT Configuration Service for nRF52. Support for nRF51 is scheduled for a future release. The application is intended to be used together with the open source [nRF Beacon for Eddystone](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone) Android App. It is recommended to read the [official specification](https://github.com/google/eddystone) for Eddystone, an open beacon format from Google to get a thorough understanding. Go to [Quick start](#quick-start) if you want to experiment right away.

<img src="https://github.com/google/eddystone/blob/master/branding/assets/png/EddyStone_final-01.png" alt="Eddystone logo" width="300px" align="middle">

#### Table of contents
* [Release note](#release-note)
* [Introduction](#introduction)
* [Supported characteristics](#supported-characteristics)
* [Requirements](#requirements)
* [Known issues](#known-issues)
* [How to install](#how-to-install)
* [How to use](#how-to-use)
* [How it works](#how-it-works)
* [Issues and support](#how-it-works)
* [Third-party crypto libraries](#third-party-cryptolibraries)
* [About](#about)
* [License](#license)

## Release note
* __v0.6__ (April 21 2016)
    * Revamped folder structures and project setup procedure to allow for better user setup experience. Namely the downloading of SDK via scripts. Details described below in [How to install](#how-to-install).
    * Merged in cifra crypto library's fix for a [known issue](https://github.com/ctz/cifra/issues/3) with EAX encryption of the eTLM frames. Now eTLM frames are properly encrypted.
    * Fixed a bug with the EID slot 4-byte clock being slow.
    * Added scan response capability when the beacon is put into connectable mode which contains `nRF5_Eddy` as the device name and the Eddystone Configuration GATT Service UUID `a3c87500-8ed3-4bdf-8a39-a01bebede295` as the UUID in the scan response packet, as recommended by the latest [spec](https://github.com/google/eddystone/tree/master/configuration-service) from Google.
    * Improved LED Indication for different beacon states: Advertising, Advertising in connectable mode, Connected. Details below in [How to use](#how-to-use).

* __v0.5__ (April 15 2016)
    * First public release


## Introduction
The new Eddystone GATT Configuration Service enables simple configuration of beacons. The user can configure the beacon to broadcast all Eddystone frame types:
* Eddystone-URL
* Eddystone-UID
* Eddystone-EID
* Eddystone-TLM
* Eddystone-eTLM

Currently the firmware has five available slots and each slot can be configured to any of the unique frame types. From the source code it is possible to increase or decrease the number of available slots.

In addition to the new Eddystone GATT Configuration Service there are also two new frame types aimed at secure use cases.

The new frame types are **Eddystone-EID** and **Eddystone-eTLM**. EID, or Ephemeral Identifier, is a secure version of UID. eTLM, or encrypted TLM, is a secure telemetry format and provides information on the health of a beacon.

Eddystone-EID and Eddystone-eTLM protect against spoofing, replay attacks and malicious asset tracking - which are known beacon vulnerabilities.

#### Spoofing
Impersonating Eddystone-EIDs is difficult since the advertising data is encrypted and regularly updated.

#### Replay Attacks
By randomizing and never sending the Unlock Key in clear text it is difficult to perform replay attacks with the new Eddystone GATT Configuration Service. The beacon creates a random challenge token every time a user tries to unlock it. The user then encrypts the Lock Key with the challenge token and sends the result to the beacon. The result will be different for every unlock and replay attacks are therefore impossible.

#### Malicious Asset Tracking
Eddystone-EIDs randomize the device ID of the beacon as well as the encrypted advertising data. Since there are no constant values to track it will be difficult if not impossible to track the location of a single beacon over any significant time period.

> **IMPORTANT**
 In order to have all the security benefits of Eddystone-EID and Eddystone-eTLM refrain from configuring other non-secure frame types while broadcasting these secure frame types


## Supported characteristics
The application supports all functionality of the Eddystone GATT Configuration Service except the advanced optional characteristics as displayed in the table below. The advanced characteristics will be implemented in a future release.

Characteristic | Name | Status
---:|---|:---:
1 | Broadcast Capabilities | :white_check_mark:
2 | Active Slot | :white_check_mark:
3 | Advertising Interval | :white_check_mark:
4 | Radio Tx Power | :white_check_mark:
5 | Advertised Tx Power (advanced) |
6 | Lock State | :white_check_mark:
7 | Unlock | :white_check_mark:
8 | Public ECDH Key | :white_check_mark:
9 | EID Identity Key | :white_check_mark:
10 | Read/Write ADV Slot | :white_check_mark:
11 | Factory Reset (advanced) |
12 | Remain Connectable (advanced) |


## Prerequisites

#### Software
* [SEGGER Embedded Studio IDE](https://www.segger.com/downloads/embeddedstudio) with the [nRF](https://devzone.nordicsemi.com/attachment/315266173907f1c16d81f842f0796730) and CMSIS packs installed. (Note: If you don't have a Keil license or you are developing on Mac OS X or Linux this is the best option).
* [Keil uVision 5 IDE](https://www.keil.com/demo/eval/arm.htm) (Note: you must have a registered version of Keil in order to compile source code that generates more than 32kB of code and data, currently this project generates 39 kB even with -O3 optimization level)
* [Git Bash](https://git-scm.com/downloads)
* [nRFgo Studio](https://www.nordicsemi.com/eng/nordic/Products/nRFgo-Studio/nRFgo-Studio-Win64/14964) (Note: Not required if using SEGGER Embedded Studio).

The application might work with other versions of the SDK/Keil but some modification of the source code is likely required on your part. For a quick start on using Embedded Studio with nRF5 devices see: https://devzone.nordicsemi.com/blogs/845/segger-embedded-studio-cross-platform-ide-w-no-cod/.

#### Hardware
* [nRF52 Development Kit](https://octopart.com/nrf52-dk-nordic+semiconductor-67145952)
* Android phone 4.3+

## Known issues
* Keil and SEGGER Embedded Studio (GCC) are currently supported. IAR and Makefile based GCC project is scheduled for a future release.
* When using SEGGER Embedded Studio two files in Nordic's SDK must be modified. These are 'retarget.c' and 'app_util_platform.h.' In 'retarget.c' lines 29&30 (FILE __stdout;) must be commented out. In 'app_util_platform.h' #define PACKED(TYPE) TYPE __attribute__((packed)) must be used instead of #define PACKED(TYPE) __packed TYPE. (These are bugs in the SDK and have been reported).
* After an Eddystone-EID slot is configured it will be preserved after power cycling. However, if you try to read the ECDH key again from the characteristic it will not be available. Slots containing other frame types are not preserved after power cycling.
* When compiling there are warnings from the third-party crypto libraries.


## How to install
#### Quick start
This is the recommended approach if you just want to get started quickly without building the project yourself.

*  Connect the nRF52 DK to your computer. It will show up as a JLINK USB drive.

*  Download the `nrf5_sdk_for_eddystone_v0.6.hex` file in the hex folder in this repository.

*  Drag and drop the `nrf5_sdk_for_eddystone_v0.6.hex` file on the JLINK drive to automatically program the nRF52 DK.

*  Install the nRF Beacon for Eddystone Android App from [Play Store](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrfbeacon.nearby).

*  Follow the [instructions on how to use the App](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone).

#### Compile from source

*  Clone this repository into your preferred location (keep it close to root to avoid path too long problem on Windows, because the SDK will be downloaded via a script into the same parent directory as well)
```
git clone https://github.com/NordicSemiconductor/nrf5-sdk-for-eddystone.git
```
* In the `nrf5-sdk-for-eddystone\setup_scripts` folder, you will find several bash/batch scripts that will help you set up the repository with the necessary components in order for Keil to compile the project. The 2 scripts that are of interest to the user are `download_sdk_setup.sh` and `existing_sdk_setup.sh`:
    *  Run the `download_sdk_setup.sh` script if you don't already have `nRF5_SDK_11.0.0_89a8197`. This script will automatically download the required nRF5 SDK into the parent folder ( so that `nRF5_SDK_11.0.0_89a8197` lives right next to `nrf5-sdk-for-eddystone`), create necessary symlinks into the SDK for Keil to find the required components, and also clone the required third party crypto libraries from Github.
    * If you already have `nRF5_SDK_11.0.0_89a8197` on your machine, then simply open `existing_sdk_setup.sh` in your favourite text editor and edit the line
    ```
    export SDK_PATH=""
    ```
    and fill in the absolute path to your existing SDK folder, as such:
    ```
    e.g. SDK_PATH="C:/example_path/nRF5_SDK_11.0.0_89a8197"
    ```
    then save it and run the script. It will do the same things as the `download_sdk_setup.sh`, except not downloading the SDK of course.

* If everything goes smoothly, you should find the following additional components in the `nrf5-sdk-for-eddystone`, indicated with ** :
```
Some_parent_folder
        nRF5_SDK_11.0.0_89a8197 (would be here if you ran download_sdk_setup.sh)
        nrf5-sdk-for-eddystone
                                bsp
                                hex
                                include
                                project
                              **sdk_components
                              **sdk_external
                                setup_scripts
                                source
                                     ble_services
                                   **crypto_libs
                                     modules
                                .gitignore
                                licenses.txt
                                README.md
```

*  Open the .emProject project file in SEGGER Embedded Studio, which is found here:
```
nrf5-sdk-for-eddystone\project\pca10040_s132\embedded_studio
```

*  Or open the .uvprojx project file in Keil, which is found here:
```
nrf5-sdk-for-eddystone\project\pca10040_s132\arm5_no_packs
```
*  The project is expected to compile with 2 warnings coming from one of the crypto libraries when using Keil. You might also need to download NordicSemiconductor::nRF_DeviceFamilyPack 8.5.0 in Keil's Pack Installer if you don't already have it before the project can compile

*  (Note: not required if using Embedded Studio) Before loading the firmware onto your nRF52 DK or starting a debug session in Keil, you must flash in the S132 Softdevice that can be found here:
```
sdk_components\softdevice\s132\hex\s132_nrf52_2.0.0_softdevice.hex
```
(Note: Not required if using Embedded Studio) The Softdevice can be flashed in with Nordic's [nRFgo Studio](https://www.nordicsemi.com/eng/nordic/Products/nRFgo-Studio/nRFgo-Studio-Win64/14964) tool. For instructions on how to use nRFgo Studio, follow the tutorial here under the [Preparing the Development Kit](https://devzone.nordicsemi.com/tutorials/2/) section.

## How to use
After flashing the firmware to a nRF52 DK it will automatically start broadcasting a Eddystone-URL pointing to http://www.nordicsemi.com, with LED 1 blinking. In order to configure the beacon to broadcast a different URL or a different frame type it is necessary to put the DK in configuration mode by pressing Button 1 on the DK so it starts advertising in "Connectable Mode". After that, it can be connected to nRF Beacon for Eddystone app, which allows the writing of the Lock Key to the Unlock Characteristic.

Please note that after pressing Button 1, the DK will only broadcast in "Connectable Mode" for 1 minute. After which, you must press Button 1 again if you did not manage to connect in time with the App.

###### LED Indications:
| LED No.       | LED State       | Beacon State  |
| ------------- |:-------------:| -----:|
| LED 1     | Blinking | Advertising |
| LED 2        | On      |   Connected to Central |
| LED 3 | On     |    Advertising in Connectable Mode |

Detailed instructions on how to use the App is available in the [nRF Beacon for Eddystone GitHub repository](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone).

## How it works
Instructions on the firmware structure and on how to modify the firmware are coming soon.

## Issues and support
This example application is provided as a firmware foundation for beacon providers or for users simply wanting to experiment with Eddystone. It is not part of the official nRF5 SDK and support is therefore limited. Expect limited follow-up of issues.

## Third-party crypto libraries
The example application uses algorithms from the following third-party cryptographic libraries.

Library | Algorithm | License
---:|---:|---|---
[uWeave](https://weave.googlesource.com/weave/libuweave) | AES-128-ECB |  [Unlicense](http://unlicense.org/)
[Cifra](https://github.com/ctz/cifra/tree/a4c29ed77990c8427e7cb8aabf3162e99c1e5daa) | AES-EAX |  [Creative Commons 0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
[Cifra](https://github.com/ctz/cifra/tree/a4c29ed77990c8427e7cb8aabf3162e99c1e5daa) | ECDH 25519 | [Creative Commons 0 1.0](https://creativecommons.org/publicdomain/zero/1.0/)
[RFC6234](https://github.com/massar/rfc6234) | HMAC-SHA256 | [Simplified BSD License](https://en.wikipedia.org/wiki/BSD_licenses#2-clause_license_.28.22Simplified_BSD_License.22_or_.22FreeBSD_License.22.29)



## About
This application has been developed by the application team at Nordic Semiconductor as a demonstration of the Eddystone GATT Configuration Service. It has not necessarily been thoroughly tested, so there might be unknown issues. It is hence provided as-is, without any warranty. However, in the hope that it still may be useful also for others than the ones we initially wrote it for, we've chosen to distribute it here on GitHub. The application is built to be used with the official nRF5 SDK, that can be downloaded from http://developer.nordicsemi.com/.

## Licenses
The nRF5 SDK for Eddystone licensing is split between the portion of the source code that
originates from Nordic Semiconductor ASA and the
portion that originates from third-parties.

* All source code, unless otherwise specified, originates from Nordic
  Semiconductor and is covered by the license present
  in documentation/license.txt in the nRF5 SDK:

* All source code contained under the following folder originates from third
  parties, and is covered by the corresponding licenses found in each of their
  respective subfolders:

    - crypto_libs

Note: The crypto_libs folder is not included in this repository but is created by running the setup script.
