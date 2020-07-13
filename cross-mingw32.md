## tdlib

```
sed s/Crypt32/crypt32/ -i tdnet/CMakeLists.txt
sed 's/Mswsock/mswsock/;s/Normaliz/normaliz/' -i tdutils/CMakeLists.txt
sed 's/WinSock2.h/winsock2.h/;s/WS2tcpip.h/ws2tcpip.h/;s/MSWSock.h/mswsock.h/;s/Windows.h/windows.h/' -i tdutils/td/utils/common.h
```

replace `if(WIN32)` with `if(CMAKE_HOST_WIN32)` for GIT_COMMIT_CMD command

```
cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DOPENSSL_FOUND=True -DOPENSSL_SSL_LIBRARY="-Wl,-Bstatic -lssl -Wl,-Bdynamic -lws2_32" -DOPENSSL_CRYPTO_LIBRARY="-Wl,-Bstatic -lcrypto -Wl,-Bdynamic -lws2_32" -DZLIB_FOUND=1 -DZLIB_LIBRARIES=/usr/i686-w64-mingw32/sys-root/mingw/lib/libz.a -DCMAKE_BUILD_TYPE=Release ..
```

Building OpenSSL 3.0:

```
./Configure --cross-compile-prefix=i686-w64-mingw32- mingw
```

## libpurple

Extract pidgin source to `deps/`

Download pidgin dependencies under `deps/win32-dev/` per http://pidgin.im/development/building/2.x.y/windows/#installing

* GTK

* libxml2

* perl

* Mozilla NSS

* SILC toolkit

* Meanwhile

* Cyrus SASL

```
cd deps/pidgin-2.13.0/libpurple/
make -fMakefile.mingw CC=i686-w64-mingw32-gcc WINDRES=i686-w64-mingw32-windres
make -fMakefile.mingw install
```

## This plugin

Additional dependencies under `deps/win32-dev/`:

* libpng

* libwebp

Build each using
```
./configure --host i686-w64-mingw32 --target i686-w64-mingw32
make
make install DESTDIR=$PWD/install
```

Building the plugin:

```
cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DTd_DIR=/w/bin/td/win32/install/usr/local/lib/cmake/Td -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++" -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" -DNoPkgConfig=True -DPurple_INCLUDE_DIRS="$PWD/../deps/pidgin-2.13.0/libpurple;$PWD/../deps/win32-dev/gtk_2_0-2.14/include/glib-2.0;$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/glib-2.0/include" -DPurple_LIBRARIES="$PWD/../deps/pidgin-2.13.0/libpurple/libpurple.dll.a;$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/libglib-2.0.dll.a;$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/libgthread-2.0.dll.a" -Dlibpng_LIBRARIES=$PWD/../deps/win32-dev/libpng-1.6.37/install/usr/local/lib/libpng16.a -Dlibwebp_INCLUDE_DIRS=$PWD/../deps/win32-dev/libwebp-1.1.0/install/usr/local/include -Dlibwebp_LIBRARIES=$PWD/../deps/win32-dev/libwebp-1.1.0/install/usr/local/lib/libwebp.a  -DPURPLE_PLUGIN_DIR=/ -DIntl_INCLUDE_DIR=$PWD/../deps/win32-dev/gtk_2_0-2.14/include -DIntl_LIBRARY=$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/libintl.dll.a -DGLIB_LIBRARIES="$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/libglib-2.0.dll.a;$PWD/../deps/win32-dev/gtk_2_0-2.14/lib/libgthread-2.0.dll.a" -DCMAKE_BUILD_TYPE=Release ..

echo ' -Wl,-Bstatic -lpthread -Wl,-Bdynamic' >>CMakeFiles/telegram-tdlib.dir/linklibs.rsp
make
i686-w64-mingw32-strip libtelegram-tdlib.dll
```

Verifying run-time dependencies:

```
strings libtelegram-tdlib.dll |grep '\.dll'
```

should contain no more than

```
%s.dll
libtelegram-tdlib.dll
libpurple.dll
libglib-2.0-0.dll
libgthread-2.0-0.dll
intl.dll
ADVAPI32.dll
CRYPT32.dll
GDI32.dll
KERNEL32.dll
msvcrt.dll
Normaliz.dll
USER32.dll
WS2_32.dll
```

## Regression test

```
WINEPATH="$PWD/../deps/win32-dev/gtk_2_0-2.14/bin;/usr/i686-w64-mingw32/sys-root/mingw/bin" wine test/tests
```

That second part of WINEPATH is for libwinpthread-1.dll, would not be needed otherwise
