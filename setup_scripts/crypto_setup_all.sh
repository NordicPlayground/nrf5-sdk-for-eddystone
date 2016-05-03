#!/bin/bash
# v0.9

cd ../source

echo "Cloning Repositories..."
git clone "https://github.com/massar/rfc6234.git" "crypto_libs/temp/rfc6234"
cd crypto_libs/temp/rfc6234
git checkout 285c8b86c0c6b8e9ffe1c420c5b09fa229629a30
cd ../../..

git clone "https://github.com/ctz/cifra.git" "crypto_libs/temp/cifra"
cd crypto_libs/temp/cifra
git checkout a4c29ed77990c8427e7cb8aabf3162e99c1e5daa
# Create a separate branch off of detached head
git checkout -b nrf5-sdk-for-eddystone
#Cifra EAX library fix
git cherry-pick 82d77cd8323f6d4473fcb68517752a778970138d
#ifndef blocks fix
git cherry-pick e4987b20d446d3dd102dbaa1df448159ed94afd6
cd ../../..



git clone "https://weave.googlesource.com/weave/libuweave" "crypto_libs/temp/tiny-aes128"
cd crypto_libs/temp/tiny-aes128
git checkout 94188148a023bde30dc6b25c01a0758ae49c98bf
cd ../../..

echo "Creating directories..."
mkdir crypto_libs/rfc6234
mkdir crypto_libs/cifra

echo "Moving files and folders..."

# tiny-aes128
mv crypto_libs/temp/tiny-aes128/third_party/tiny-aes128-c crypto_libs

# cifra
mv crypto_libs/temp/cifra/src/ext/handy.h crypto_libs/cifra
mv crypto_libs/temp/cifra/src/*.h crypto_libs/cifra
mv crypto_libs/temp/cifra/src/*.c crypto_libs/cifra

# rfc6234
mv crypto_libs/temp/rfc6234/*.txt crypto_libs/rfc6234
mv crypto_libs/temp/rfc6234/*.c crypto_libs/rfc6234
mv crypto_libs/temp/rfc6234/*.h crypto_libs/rfc6234

# find and replace MIN/MAX defines
sed -i 's/#define MIN(x, y) \\/#ifndef   MIN\n  #define MIN(a, b)         (((a) < (b)) ? (a) : (b))\n#endif/' crypto_libs/cifra/handy.h
sed -i 's/#define MAX(x, y) \\/#ifndef   MAX\n  #define MAX(a, b)         (((a) > (b)) ? (a) : (b))\n#endif/' crypto_libs/cifra/handy.h
sed -i '/  ({ typeof (x) __x = (x); \\/d' crypto_libs/cifra/handy.h
sed -i '/     typeof (y) __y = (y); \\/d' crypto_libs/cifra/handy.h
sed -i '/     __x < __y ? __x : __y; })/d' crypto_libs/cifra/handy.h
sed -i '/     __x > __y ? __x : __y; })/d' crypto_libs/cifra/handy.h

echo "Cleaning up..."
rm -rf crypto_libs/temp

echo "Finished successfully"
