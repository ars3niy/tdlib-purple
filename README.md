# New libpurple plugin for Telegram

## Motivation

telegram-purple seems to miss incoming messages a lot, thus writing new plugin using latest tdlib.

## Functionality

It should be just about usable for private chats.

## Building

TDLib should be prebuilt and installed somewhere (requires C++14):
```
cd <path to TDLib sources>
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install DESTDIR=/path/to/tdlib
```
Also see [building](https://github.com/tdlib/td#building) for additional details on TDLib building.

Building this plugin:
```
mkdir build
cd build
cmake -DTd_DIR=/path/to/tdlib/usr/local/lib/cmake/Td ..
make
```

## Installation

Copy the .so to libpurple plugins directory.

It's good to have telegram-purple installed as well since its icon is used at the moment.
