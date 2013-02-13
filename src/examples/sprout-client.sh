#! /bin/bash

if [ $# -lt 3 ] ; then
  echo "Enter address and port of relay and qdisc "
  exit;
fi
relay_address=$1
relay_port=$2
qdisc=$3

set -x
sudo tunctl -t tap0
sudo ifconfig tap0 10.0.0.1 netmask 255.255.255.0 up
sudo ifconfig tap0 mtu 1420
sudo route add default gw 10.0.0.2 metric 2 # ensure eth0 is preferred to tap0
sudo ./sproutbt2 $relay_address $relay_port $qdisc
