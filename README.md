# New libpurple plugin for Telegram

This is a future successor to [telegram-purple](https://github.com/majn/telegram-purple).

There is a [telegram group](https://t.me/joinchat/BuRiSBO0mMw7Lxy0ufVO5g) (what else?) for discussions and support.

## Functionality

For missing features that someone cared about, see the list of issues:
* https://github.com/ars3niy/tdlib-purple/issues?q=is%3Aissue+is%3Aopen+label%3A%22missing+feature%22
* https://github.com/ars3niy/tdlib-purple/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement

### Animated stickers

Converting animated stickers to GIFs is CPU-intensive. If this is a problem,
the conversion can be disabled in account settings, or even at compile time (see below).

## Installation

Binary packages for Debian, Fedora, openSUSE and Ubuntu are available at https://download.opensuse.org/repositories/home:/ars3n1y/ .

AUR package for Arch: https://aur.archlinux.org/packages/telegram-tdlib-purple-git/

Windows build is available from https://eion.robbmob.com/tdlib/ (copy libtelegram-tdlib.dll to libpurple plugins directory).

Alternatively, build from source (see below).

## Debugging vs. privacy

It's good to have debug log at hand whenever a glitch is observed, or to be able to reproduce the glitch with loggin turned on. With pidgin, debug log can be turn on like this:
```
pidgin -d >&~/pidgin.log
```

The debug log contains a lot of private information such as names and phone numbers of all contacts, list of all channels you've participated in or text of all sent and received messages. Be mindful of that before posting debug log on the internets. Even just saving debug log to a file can be a questionable idea if there are multiple users on the system (since permissions will be 0644 by default). Such is the nature of debugging instant messaging software.

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

This will build with test API id, which is supposed to be rate limited and may
occasionally not work. Maybe I will man up and include my API id in the source
code in the future. For now, it's possible to build with `-DAPI_ID=<api id> -DAPI_HASH=<api hash>`
to use another API id.

To install, copy the .so to libpurple plugins directory, or run `make install`.

Building using existing librlottie: `-DNoBundledLottie=True`

Building without animated sticker decoding: `-DNoLottie=True`

Building without localization: `-DNoTranslations=True`

## Proper user names in bitlbee

```
account telegram-tdlib set nick_format %full_name
```

## Regression test

Build google test library and `make install` it somewhere

Run cmake with `-DGTEST_PATH=/path/to/gtest`

`make run-tests` or `make tests`, `test/tests` or `valgrind test/tests`

## GPL compatibility: building tdlib with OpenSSL 3.0

OpenSSL versions prior to 3.0 branch have license with advertisement clause, making it incompatible with GPL. If this is a concern, a possible solution is to build with OpenSSL 3.0 which uses Apache 2.0 license.

### Building OpenSSL

Remove quotes and spaces from `RELEASE_DATE` in VERSION.

Replace `OPENSSL_VERSION_NUMBER` definition with `#define OPENSSL_VERSION_NUMBER 0x30000000L` in include/openssl/opensslv.h.in (not always necessary, depending on cmake version).

```
./config --prefix=/path/to/openssl
make
make install
rm /path/to/openssl/lib/*.so*
```

### Building tdlib

Same as usual, but with additional cmake argument `-DOPENSSL_ROOT_DIR=/path/to/openssl`

If build fails due to linker errors with dlopen etc. not found then

```
sed 's/tdnet/tdcore tdnet/' -i benchmark/CMakeLists.txt
```
