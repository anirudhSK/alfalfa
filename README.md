Alfalfa is a research project to build a videoconferencing system
that works well over cellular wireless networks. It uses the same
SSP protocol as Mosh, the mobile shell.

See http://alfalfa.mit.edu for instructions.

## Prerequisites (Debian/Ubuntu):
```
build-essential
autotools-dev
libboost-math1.50-dev 
libboost-math1.50.0 
libprotobuf7 or libprotobuf8
libprotobuf-dev 
```
## Runtime requisites:
```
uml-utilities
```

## How to build (Linux):
```
./autogen.sh
./configure --enable-examples
LANG=C make -j3
```

## Build client/server
```
cd source/examples
make sproutbt2
```

## How to build (OS X):
```
cd macosx
./buildsh
```

This fails, SO_BINDTODEVICE does not exist on OS X. This would be needed to bind to a specific network interface (ex: eth0, en1). On OS X you can bind to an IP address.

## How to run:

Server
```
./src/examples/sproutbt2 qdisc interface_name client_mac
```

Practical example:
```
sudo tunctl -t tap-relay
sudo ifconfig tap-relay 10.0.0.1 netmask 255.255.255.0 up
sudo ifconfig tap-relay mtu 1500
sudo ethtool --offload  tap-relay gso off  tso off gro off ufo off lro off
sudo ethtool --offload eth0 gso off tso off gro off ufo off lro off
./src/examples/sproutbt2 Sprout eth0 00:aa:22:dd:55:33
```

Server mode output:
```
Interface eth0 has index 2
Port bound is 60001
qdisc is 1 
```

Client
```
./src/examples/sproutbt2 ip port qdisc interface_name client_mac
```

```
sudo tunctl -t tap-client
sudo ifconfig tap-client 10.0.0.2 netmask 255.255.255.0 up
sudo ifconfig tap-client mtu 1500
sudo ethtool --offload  tap-client gso off  tso off gro off ufo off lro off
sudo ethtool --offload eth0 gso off tso off gro off ufo off lro off
sudo ./sproutbt2 10.0.0.1 60001 Sprout eth0 00:1a:92:9d:69:85
```

Snips from client mode output:
```
Interface eth0 has index 2
Port bound is 0
qdisc is 1 
Looping...
Current forecast: 2 4 7 9 12 15 17
 Some other protocol type, packet is 26e486e8 
 Some other protocol type, packet is 26e486e8 
Current forecast: 2 4 7 9 12 15 17
 Some other protocol type, packet is 27ebd078 
 Some other protocol type, packet is 27ebd078 
```

Arguments:
> qdisc, either 'CoDel' or 'Sprout'
