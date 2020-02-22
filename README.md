# New libpurple plugin for Telegram

Early prototype, not functional yet.

Implemented so far:

* Logging in with previously registered account
* Populating contact list (private chats only, no groups, no channels)
* Some outgoing messages from another client (re-displayed at every login)
* Unread incoming messages (re-displayed at every login)

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
