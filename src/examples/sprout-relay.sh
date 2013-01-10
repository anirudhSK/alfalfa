#! /bin/bash

set -x
sudo tunctl -t tap0
sudo ifconfig tap0 10.0.0.2 netmask 255.255.255.0 up
sudo ./sproutbt2 # binds to eth0 and tap0
