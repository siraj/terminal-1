#!/bin/sh

set -o errexit  # exit on error
set -o nounset  # trigger error when expanding unset variables

dstpath="$1"
curpath="$(dirname "$(readlink -f "$0")")"

binpath="$curpath/../build_terminal/Release/bin"
binary="$binpath/blocksettle"
scriptpath="$curpath/../Scripts"

libprotobuf="${DEV_3RD_ROOT="$curpath/../../3rd"}/release/Protobuf/lib"

if [ ! -x $binary ]; then
    echo "Release terminal binary $binary doesn't exist!"
    exit
fi

if [ ! -d $libprotobuf ]; then
   echo "Protobuf library dir is missing at $libprotobuf!"
   exit
fi

rm -rf "$dstpath"
cp -ra "$curpath/Ubuntu/" "$dstpath"

mkdir -p "$dstpath/usr/bin"
mkdir -p "$dstpath/lib/x86_64-linux-gnu"
rm -f "$dstpath/usr/bin/RFQBot.qml"
rm -rf "$dstpath/usr/share/blocksettle/scripts"
mkdir -p "$dstpath/usr/share/blocksettle/scripts"

cp "$binpath/blocksettle" "$dstpath/usr/bin/"
cp "$binpath/blocksettle_signer" "$dstpath/usr/bin/"
cp "$scriptpath/DealerAutoQuote.qml" "$dstpath/usr/share/blocksettle/scripts/"
cp "$scriptpath/RFQBot.qml" "$dstpath/usr/share/blocksettle/scripts/"
cp -P "$libprotobuf/libprotobuf.so"* "$dstpath/lib/x86_64-linux-gnu/"
cp "$libprotobuf/libprotobuf.la" "$dstpath/lib/x86_64-linux-gnu/"
