# nRF5 SDK for Eddystone™

This is an example implementation of the Eddystone GATT Configuration Service for nRF52. Support for nRF51 is scheduled for a future release. It is recommended to read the [official specification](https://github.com/google/eddystone) for Eddystone, an open beacon format from Google.

This application is intended to be used together with the open source **nRF Beacon for Eddystone** Android App.

<img src="https://github.com/google/eddystone/blob/master/branding/assets/png/EddyStone_final-02.png" alt="Eddystone logo" width="200px">
 ---
#### Table of contents
* [Introduction](#introduction)
* [Supported characteristics](#supported-characteristics)
* [Requirements](#requirements)
* [How to install](#how-to-install)
* [How to use](#how-to-use)
* [How it works](#how-it-works)
* [Issues and support](#how-it-works)
* [Cryptographic libraries](#cryptographic-libraries)
* [About](#about)
* [License](#license)

## Introduction
The new Eddystone GATT Configuration Service enables simple configuration of beacons. The user can configure the beacon to broadcast all Eddystone frame types:
* Eddystone-URL
* Eddystone-UID
* Eddystone-EID
* Eddystone-TLM
* Eddystone-eTLM

Each frame type is given a dedicated slot and it is possible to set unique advertising intervals for each slot. This means that a beacon can send a Eddystone-URL every two seconds and a Eddystone-UID every second or any other combination. The current firmware has a lower limit of 100ms for advertising interval. It will be up to the user or developer to choose values that best fit with the use case and desired power consumption.

In addition to the new Eddystone GATT Configuration Service there are also two new frame types aimed at secure use cases.

The new frame types are **Eddystone-EID** and **Eddystone-eTLM**. EID, or Ephemeral Identifier, is a secure version of UID. eTLM, or encrypted TLM, is a secure telemetry format and provides information on the health of a beacon.

Eddystone-EID and Eddystone-eTLM protect against spoofing, replay attacks and malicious asset tracking - which are known beacon vulnerabilities.

#### Spoofing
Beacon spoofing is pretending to be a beacon. With regular beacons it is possible for someone to hijack the advertising data, which is always the same, and connect it to their own app. This could for instance enable an app for company A to serve information when a user is near a beacon for company B.

This is impossible with Eddystone-EID since the advertising data is encrypted and regularly updated.

#### Replay Attacks
If a beacon is unlocked with a fixed value it is possible to listen to the radio communication between a phone and a beacon and record the unlock value. A malicious user could now replay this unlock value to the beacon and gain configuration access.

By randomizing and never sending the Unlock Key in clear text it is impossible to perform replay attacks with the new Eddystone GATT Configuration Service. The beacon creates a random challenge token every time a user tries to unlock it. The user then encrypts the Lock Key with the challenge token and sends the result to the beacon. The result will be different for every unlock and replay attacks are therefore impossible.

Unauthorized authentication
Unauthorized authentication could allow someone to connect to and configure a beacon. This way company A could simply reconfigure a beacon owned by company B to advertise company A data.

The new Eddystone GATT Configuration Service is locked down by default. In order to configure the beacon a user must write a Lock Key to the Unlock Characteristic. The Lock Key is unique to each beacon and it will not be possible to remotely reconfigure the beacon without the Lock Key. Note that the Lock Key is encrypted with a random challenge token before it is sent.

#### Malicious Asset Tracking
If a beacon is attached to a bus it could be possible to track the location of the beacon and therefore the bus. This is especially true if the beacon is always advertising the same data.

Eddystone-EID randomizes the device ID of the beacon as well as the encrypted advertising data. Since there are no constant values to track it will be difficult if not impossible to track the location of a single beacon over any significant time period.

> **IMPORTANT**
A beacon should only be configured as Eddystone-EID and Eddystone-eTLM slots in order to have all the security benefits. A beacon configured as both Eddystone-EID and Eddystone-UID would still be vulnerable to tracking.



## Supported characteristics
The application supports all functionality of the Eddystone Configuration Service except the advanced optional characteristics as displayed in the table below.

Characteristic | Name | Status
---:|---|---
1 | Broadcast Capabilities | :white_check_mark:
2 | Active Slot | :white_check_mark:
3 | Advertising Interval | :white_check_mark:
4 | Radio Tx Power | :white_check_mark:
5 | Advertised Tx Power (advanced) |
6 | Lock State | :white_check_mark:
7 | Unlock | :white_check_mark:
8 | Public ECDH Key | :white_check_mark:
9 | EID Identity Key | :white_check_mark:
10 | ADV Slot | :white_check_mark:
11 | Factory Reset (advanced) |
12 | Remain Connectable (advanced) |


## Requirements

#### Software
* [nRF5 SDK 11](http://developer.nordicsemi.com/nRF5_SDK/nRF5_SDK_v11.x.x/)

The application might work with other versions of the SDK but some modification of the source code is likely required on your part.

#### Hardware
* [nRF52 Development Kit](https://octopart.com/nrf52-dk-nordic+semiconductor-67145952)

## How to install
#### Quick Start
This is the recommended approach if you just want to get started quickly without building the project yourself.
1. Connect the nRF52 DK to your computer. It will show up as a JLINK drive.
2. Download the some_name.hex file in the hex folder in this repository.
3. Drag and drop the .hex file to the JLINK drive to flash the nRF52 DK.
4. Install the nRF Beacon for Eddystone Android app from Play Store.
5. Follow the instructions in [How to use](#how-to-use)

#### Compile from source
1. Download the nRF5 SDK 11 and extract to a suitable location. On Windows we recommend placing it close to (C:) to avoid long folder and file name problems.
2. Download and extract this repository
3. Copy the nrf5_eddystone_sdk folder and place it next to the other folders in the nRF5 SDK 11 ble_peripheral folder.
4. More to come...

## How to use
After flashing the firmware to a nRF52 DK it will automatically start broadcasting a Eddystone-URL pointing to this repository. In order to configure the beacon to broadcast a different URL or a different frame type it is necessary to put the DK in configuration mode and write the Lock Key to the Unlock Characteristic. Download and install the

Roshan goes here


## How it works
Tony goes here

## Issues and support
This example application is provided as a source code foundation for beacon providers or for users simply wanting to experiment with Eddystone. It is not part of the official nRF SDK and support is therefore limited. Expect limited follow-up of issues.

## Cryptographic libraries
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

* All source code contained under the following folders originates from Nordic
  Semiconductor and is covered by the license present
  in documentation/license.txt in the nRF5 SDK:

    - some
    - example
    - folders

* All source code contained under the following folder originates from third
  parties, and is covered by the corresponding licenses found in each of their
  respective subfolders:

    - crypto_libs

* The crypto_libs folder is created by running the crypto_libs.bat program.
