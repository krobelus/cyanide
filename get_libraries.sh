#!/bin/sh

repo='http://repo.merproject.org/obs/home:/krobelus/sailfish_latest_armv7hl/armv7hl/'

rm -rf res
mkdir  res
cd     res

die() {
    echo "$@"
    exit 1
}

which wget || die "wget not found"
which rpm2cpio || die "rpm2cpio not found"
which bsdtar || die "bsdtar not found"

rpmextract() {
    rpm2cpio "$1" | bsdtar -xf -
}

cat << EOF | while read package; do wget $repo$package; done
filter_audio-15.0211-10.8.1.jolla.armv7hl.rpm
filter_audio-devel-15.0211-10.8.1.jolla.armv7hl.rpm
libsodium-1.0.2-10.18.1.jolla.armv7hl.rpm
libsodium-devel-1.0.2-10.18.1.jolla.armv7hl.rpm
libv4l-1.6.2-10.29.1.jolla.armv7hl.rpm
libv4l-devel-1.6.2-10.29.1.jolla.armv7hl.rpm
libvpx-1.3.0-10.20.1.jolla.armv7hl.rpm
libvpx-devel-1.3.0-10.20.1.jolla.armv7hl.rpm
libvpx-utils-1.3.0-10.20.1.jolla.armv7hl.rpm
opus-1.1-10.5.1.jolla.armv7hl.rpm
opus-devel-1.1-10.5.1.jolla.armv7hl.rpm
toxcore-15.0610-10.3.1.jolla.armv7hl.rpm
toxcore-devel-15.0610-10.3.1.jolla.armv7hl.rpm
EOF

for package in *.rpm; do
    rpmextract "$package"
done
