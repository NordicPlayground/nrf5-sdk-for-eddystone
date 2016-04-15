#!/bin/bash
set -e

echo "Cloning Repositories..."
git clone "https://github.com/massar/rfc6234.git" "crypto_libs/temp/rfc6234"
git clone "https://github.com/ctz/cifra.git" "crypto_libs/temp/cifra"
git clone "https://weave.googlesource.com/weave/libuweave" "crypto_libs/temp/tiny-aes128"

echo "Creating directories..."
mkdir crypto_libs/rfc6234
mkdir crypto_libs/cifra

echo "Moving files and folders..."
mv crypto_libs/temp/tiny-aes128/third_party/tiny-aes128-c crypto_libs
mv crypto_libs/temp/cifra/src/*.c crypto_libs/cifra
mv crypto_libs/temp/cifra/src/*.h crypto_libs/cifra
mv crypto_libs/temp/rfc6234/*.c crypto_libs/rfc6234
mv crypto_libs/temp/rfc6234/*.h crypto_libs/rfc6234
mv crypto_libs/temp/rfc6234/*.txt crypto_libs/rfc6234

echo "Cleaning up..."
rm -rf crypto_libs/temp

echo "Finished successfully"
