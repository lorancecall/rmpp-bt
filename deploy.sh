#!/bin/sh
# Push build/ to the device (ssh alias rmpp) and restart xochitl.
# Never `systemctl stop xochitl`: stopping it powers the tablet off.
set -e
cd "$(dirname "$0")"
scp -q build/librmpp-bt.so build/rmpp-bt.rcc rmpp:/home/root/
ssh rmpp 'set -e
mount -o remount,rw /
mkdir -p /usr/lib/rmpp-bt
for f in librmpp-bt.so rmpp-bt.rcc; do cp /home/root/$f /usr/lib/rmpp-bt/$f.new; mv /usr/lib/rmpp-bt/$f.new /usr/lib/rmpp-bt/$f; done
sync
systemctl reset-failed xochitl || true
systemctl restart xochitl
sleep 8
mount -o remount,ro / || echo "WARN: root still rw"
systemctl is-active xochitl'
