# nRF5 SDK for Eddystone™

NOTE: Source code is coming soon. You may still follow the [Quick start](#quick-start) guide to flash your nRF52 DK with the firmware.

This is an example implementation of the Eddystone GATT Configuration Service for nRF52. Support for nRF51 is scheduled for a future release. The application is intended to be used together with the open source [nRF Beacon for Eddystone](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone) Android App. It is recommended to read the [official specification](https://github.com/google/eddystone) for Eddystone, an open beacon format from Google to get a thorough understanding. Go to [Quick start](#quick-start) if you want to experiment right away.

<img src="https://github.com/google/eddystone/blob/master/branding/assets/png/EddyStone_final-01.png" alt="Eddystone logo" width="300px" align="middle">

#### Table of contents
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
 In order to have all the security benefits of Eddystone-EID and Eddystone-eTLM refrain from configuring other frame types while broadcasting.


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
* [nRF5 SDK 11](http://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v11.x.x/)
* [Keil uVision 5 IDE](https://www.keil.com/demo/eval/arm.htm)
* [Git Bash](https://git-scm.com/downloads)
* [nRFgo Studio](https://www.nordicsemi.com/eng/nordic/Products/nRFgo-Studio/nRFgo-Studio-Win64/14964)

The application might work with other versions of the SDK/Keil but some modification of the source code is likely required on your part.

#### Hardware
* [nRF52 Development Kit](https://octopart.com/nrf52-dk-nordic+semiconductor-67145952)
* Android phone 4.3+

## Known issues
* Only Keil is supported for now. GCC and IAR are  scheduled for a future release.
* Only Windows development environment is supported for now. Linux and OSX are scheduled for a later release. You may still flash the firmware using the [Quick start](#quick-start) guide.
* eTLM encryption library has a known [issue](https://github.com/ctz/cifra/issues/3).
* After an Eddystone-EID slot is configured it will be preserved after power cycling. However, if you try to read the ECDH key again from the characteristic it will not be available. Slots containing other frame types are not preserved after power cycling.
* When compiling there are warnings from the third-party crypto libraries.

## How to install
#### Quick start
This is the recommended approach if you just want to get started quickly without building the project yourself.

1. Connect the nRF52 DK to your computer. It will show up as a JLINK USB drive.

2. Download the `nrf5_sdk_for_eddystone.hex` file in the hex folder in this repository.

3. Drag and drop the `nrf5_sdk_for_eddystone.hex` file on the JLINK drive to automatically program the nRF52 DK.

4. Install the nRF Beacon for Eddystone Android App from [Play Store](https://play.google.com/store/apps/details?id=no.nordicsemi.android.nrfbeacon.nearby) (available soon).

5. Follow the [instructions on how to use the App](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone).

#### Compile from source
1. Download the [nRF5 Software Development Kit v11](http://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v11.x.x/) and extract to a suitable location. We recommend placing it close to root to avoid problems related to long folder and file names.

2. Clone this repository into the SDK under ...nRF5_SDK_11.0.0_xxxxxxx\examples\ble_peripheral
```
git clone https://github.com/NordicSemiconductor/nrf5-sdk-for-eddystone.git
```
So the resulting folder structure looks like this:
```
examples
        ble_peripheral
                ble_app_alert_notification
                ble_app_ancs_c
                ble_app_beacon
                ....
                nrf_sdk_for_eddystone
```
4. Run the `crypto_setup_all.sh` script in the `nrf5_sdk_for_eddystone\source` folder. This will clone and configure third-party cryptographic libraries.

5. Open the .uvprojx project file in Keil, which is found here:
```
nrf5-sdk-for-eddystone\project\pca10040_s132\arm5_no_packs
```
6. The project is expected to compile with 2 warnings coming from one of the crypto libraries

7. Before loading the firmware onto your nRF52 DK or starting a debug session in Keil, you must flash in the S132 Softdevice that can be found here:
```
components\softdevice\s132\hex\s132_nrf52_2.0.0_softdevice.hex
```
The Softdevice can be flashed in with Nordic's [nRFgo Studio](https://www.nordicsemi.com/eng/nordic/Products/nRFgo-Studio/nRFgo-Studio-Win64/14964) tool. For instructions on how to use nRFgo Studio, follow the tutorial here under the [Preparing the Development Kit](https://devzone.nordicsemi.com/tutorials/2/) section.

## How to use
After flashing the firmware to a nRF52 DK it will automatically start broadcasting a Eddystone-URL pointing to http://www.nordicsemi.com. In order to configure the beacon to broadcast a different URL or a different frame type it is necessary to put the DK in configuration mode and write the Lock Key to the Unlock Characteristic. This is done by using the nRF Beacon for Eddystone App.

Detailed instructions on how to use the App is available in the [nRF Beacon for Eddystone GitHub repository](https://github.com/NordicSemiconductor/Android-nRF-Beacon-for-Eddystone).

## How it works
Instructions on how to modify the firmware are coming soon.

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

Note: The crypto_libs folder is not included in this repository but is created by running the crypto_setup script.
