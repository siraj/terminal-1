#!/bin/sh

set -o errexit  # exit on error
set -o nounset  # trigger error when expanding unset variables

dir="$(dirname "$(readlink -f "$0")")"

"$dir/debian.sh" ./debian-tmp

dpkg -b ./debian-tmp ./bsterminal.deb
echo "deb package generated"

rm -rf ./debian-tmp
