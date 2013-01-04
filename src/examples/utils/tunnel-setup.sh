#! /usr/bin/bash

# On relay :
sudo tunctl -t tap0 -u user_name
sudo ifconfig tap0 10.0.0.2 netmask 255.255.255.0 up

# On client :
sudo tunctl -t tap0 -u user_name
sudo ifconfig tap0 10.0.0.1 netmask 255.255.255.0 up

# On client :
sudo route add default gw 10.0.0.2 metric 2 # ensure eth0 is preferred to tap0

# On relay , set up NAT (change this to a bridge later to give client a public IP) :
# flush
sudo iptables -t nat -F
sudo iptables -F
# setup
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
sudo iptables -A FORWARD -m state --state RELATED,ESTABLISHED -j ACCEPT -i eth0 -o tap0
sudo iptables -A FORWARD -j ACCEPT -o rmnet0 -i rndis0

# Now run sprout :
# On relay :
sudo ./sproutbt2 # binds to eth0 and tap0

# On client :
sudo ./sproutbt2 relay_address 60001
