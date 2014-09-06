Alfalfa is a research project to build a videoconferencing system
that works well over cellular wireless networks. It uses the same
SSP protocol as Mosh, the mobile shell.

## Prerequisites (Debian/Ubuntu):
```
build-essential
autotools-dev
libboost-all-dev
```
## Runtime requisites:
```
uml-utilities
```

## How to build (Linux):
```
./autogen.sh
./configure
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

This fails, SO_BINDTODEVICE does not exist on OS X
