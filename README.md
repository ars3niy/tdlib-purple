# New libpurple plugin for Telegram

Tdlib-purple is the successor to [telegram-purple](https://github.com/majn/telegram-purple).

This particular repository collects all [seemingly-working PRs on ars3ny's original repository](https://github.com/ars3niy/tdlib-purple/pulls).

In particular, it includes:
- [#163](https://github.com/ars3niy/tdlib-purple/pull/163) (non-member group chats)
- [#159](https://github.com/ars3niy/tdlib-purple/pull/159) (display self-destruct messages)
- [#154](https://github.com/ars3niy/tdlib-purple/pull/154) (tdlib 1.8.0)
- [#133](https://github.com/ars3niy/tdlib-purple/pull/133) (`build_and_install.sh`)
- [#129](https://github.com/ars3niy/tdlib-purple/pull/129) (configurable API id and hash)
- NOT [#110](https://github.com/ars3niy/tdlib-purple/pull/110), because I'm not entirely sure yet why it's necessary
- NOT [#100](https://github.com/ars3niy/tdlib-purple/pull/100), because I'm lazy (feel free to make a PR if you promise me that it really works)

There is a [telegram group](https://t.me/joinchat/BuRiSBO0mMw7Lxy0ufVO5g) (what else?) for discussions and support.

## Functionality

For missing features that someone cared about, see the list of issues:
* https://github.com/ars3niy/tdlib-purple/issues?q=is%3Aissue+is%3Aopen+label%3A%22missing+feature%22
* https://github.com/ars3niy/tdlib-purple/issues?q=is%3Aissue+is%3Aopen+label%3Aenhancement

### Animated stickers

Converting animated stickers to GIFs is CPU-intensive. If this is a problem,
the conversion can be disabled in account settings, or even at compile time (see below).

## Installation

You can easily build from source:
- Make sure you already have installed g++, cmake, git, pkg-config.
- Install the development packages for purple, webp, openssl, and png, using your OS's package manager
-    For Debian systems, like Ubuntu, install these build dependencies like this:
  `sudo apt install  libpurple-dev libwebp-dev libpng-dev g++ cmake git pkg-config gettext libssl-dev`
- Run `./build_and_install.sh` to build, it will ask for your sudo password just before installing tdlib-purple systemwide
- Restart pidgin to load the new plugin.

The script may fail with the following error:
```
-- Found PkgConfig: /usr/bin/pkg-config (found version "1.8.1") 
-- Checking for module 'purple'
--   Package 'purple', required by 'virtual:world', not found
CMake Error at /usr/share/cmake-3.25/Modules/FindPkgConfig.cmake:607 (message):
  A required package was not found
Call Stack (most recent call first):
  /usr/share/cmake-3.25/Modules/FindPkgConfig.cmake:829 (_pkg_check_modules_internal)
  CMakeLists.txt:27 (pkg_check_modules)
```
This indicates that pkgconfig is confused and cannot find the installed package-config files. This can have several reasons:
- Perhaps you forgot to install the development packages, see above.
- Perhaps your system is actually in the middle of a migration of sorts. This seems to be the case at the time of writing with Debian Testing. In this case, try to build with `PKG_CONFIG_PATH=/usr/lib/x86_64-linux-gnu/pkgconfig/ ./build_and_install.sh`

You may sometimes need to rebuild the plugin after certain OS updates.

## Debugging vs. privacy

It's good to have debug log at hand whenever a glitch is observed, or to be able to reproduce the glitch with loggin turned on. With pidgin, debug log can be turn on like this:
```
pidgin -d >&~/pidgin.log
```

The debug log contains a lot of private information such as names and phone numbers of all contacts, list of all channels you've participated in or text of all sent and received messages. Be mindful of that before posting debug log on the internets. Even just saving debug log to a file can be a questionable idea if there are multiple users on the system (since permissions will be 0644 by default). Such is the nature of debugging instant messaging software.

## Building by hand

Note that you will only need to do this in rare circumstances, or if you have special requirements.
It is often a better idea to instead modify `./build_and_install.sh` to use the flags you like.

Compatible version of TDLib should be prebuilt and installed somewhere (requires C++14). Version requirement can be found in CMakeLists.txt:
```
grep -o "tdlib version.*" CMakeLists.txt
```

TDLib can be built like this:
```
cd <path to TDLib sources>
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
make install DESTDIR=/path/to/tdlib
```

See also [building](https://github.com/tdlib/td#building).

libtgvoip is required for voice calls.

Building this plugin:
```
mkdir build
cd build
cmake -DTd_DIR=/path/to/tdlib/usr/local/lib/cmake/Td ..
make
```

To build with an alternate default API id (which may occasionally break or be
rate-limited), the user can specify `-DAPI_ID=<api id> -DAPI_HASH=<api hash>`
as CMake options at compile time.

To install, copy the .so to libpurple plugins directory, or run `make install`.

Building using existing librlottie: `-DNoBundledLottie=True`

Building without animated sticker decoding: `-DNoLottie=True`

Building without localization: `-DNoTranslations=True`

Building without voice call support: `-DNoVoip=True` (This is the default for `./build_and_install.sh`)

Building with voice call support: `-Dtgvoip_LIBRARIES="tgvoip;opus;<any other tgvoip dependencies>"`

If libtgvoip is not installed in include/library path then build with
```
-Dtgvoip_INCLUDE_DIRS=/path/to/tgvoip/include -Dtgvoip_LIBRARIES="/path/to/libtgvoip.a;<dependencies>"
```

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
