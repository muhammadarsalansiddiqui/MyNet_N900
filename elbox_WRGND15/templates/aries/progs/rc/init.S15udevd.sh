#!/bin/sh
mkdir -p /var/tmp/storage
mount -t tmpfs tmpfs /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mkdir -p /dev/net
udevd --daemon
udevstart
