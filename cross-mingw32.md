## tdlib

```
sed s/Crypt32/crypt32/ -i tdnet/CMakeLists.txt
sed 's/Mswsock/mswsock/;s/Normaliz/normaliz/' -i tdutils/CMakeLists.txt
sed 's/WinSock2.h/winsock2.h/;s/WS2tcpip.h/ws2tcpip.h/;s/MSWSock.h/mswsock.h/;s/Windows.h/windows.h/' -i tdutils/td/utils/common.h
```

replace `if(WIN32)` with `if(CMAKE_HOST_WIN32)` for GIT_COMMIT_CMD command

```
cmake -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
    -DOPENSSL_FOUND=True \
    -DOPENSSL_SSL_LIBRARY="-Wl,-Bstatic -lssl -Wl,-Bdynamic -lws2_32" \
    -DOPENSSL_CRYPTO_LIBRARY="-Wl,-Bstatic -lcrypto -Wl,-Bdynamic -lws2_32" \
    -DZLIB_FOUND=1 -DZLIB_LIBRARIES=/usr/i686-w64-mingw32/sys-root/mingw/lib/libz.a \
    -DCMAKE_BUILD_TYPE=Release ..
make
make install DESTDIR=../install
```

Building OpenSSL 3.0:

```
./Configure --cross-compile-prefix=i686-w64-mingw32- mingw
```

## libpurple

Extract pidgin source to say `../deps/`

Download pidgin dependencies under `../deps/win32-dev/` per http://pidgin.im/development/building/2.x.y/windows/#installing

* GTK

* libxml2

* perl

* Mozilla NSS

* SILC toolkit

* Meanwhile

* Cyrus SASL

```
cd ../deps/pidgin-2.13.0/libpurple/
make -fMakefile.mingw CC=i686-w64-mingw32-gcc WINDRES=i686-w64-mingw32-windres
make -fMakefile.mingw install
```

## This plugin

Additional dependencies under `../deps/win32-dev/`:

* libpng

* libwebp

* opus

* webrtc-audio-processing

* libtgvoip from https://github.com/ars3niy/libtgvoip

Build first three using
```
./configure --host i686-w64-mingw32 --target i686-w64-mingw32
make
make install DESTDIR=$PWD/install
```

For opus:
```
CFLAGS=-D_FORTIFY_SOURCE=0 ./configure --host i686-w64-mingw32 --target i686-w64-mingw32
```

Building webrtc library:
```
diff --git a/webrtc/base/platform_thread.cc b/webrtc/base/platform_thread.cc
index 707ccf8..711c45f 100644
--- a/webrtc/base/platform_thread.cc
+++ b/webrtc/base/platform_thread.cc
@@ -63,7 +63,7 @@ bool IsThreadRefEqual(const PlatformThreadRef& a, const PlatformThreadRef& b) {
 
 void SetCurrentThreadName(const char* name) {
   RTC_DCHECK(strlen(name) < 64);
-#if defined(WEBRTC_WIN)
+#if defined(_MSC_VER)
   struct {
     DWORD dwType;
     LPCSTR szName;
diff --git a/webrtc/modules/audio_coding/codecs/isac/main/source/os_specific_inline.h b/webrtc/modules/audio_coding/codecs/isac
/main/source/os_specific_inline.h
index 2b446e9..8e64f98 100644
--- a/webrtc/modules/audio_coding/codecs/isac/main/source/os_specific_inline.h
+++ b/webrtc/modules/audio_coding/codecs/isac/main/source/os_specific_inline.h
@@ -15,9 +15,9 @@
 #include <math.h>
 #include "webrtc/typedefs.h"
 
-#if defined(WEBRTC_POSIX)
+#if (defined(WEBRTC_POSIX) || defined(__GNUC__))
 #define WebRtcIsac_lrint lrint
-#elif (defined(WEBRTC_ARCH_X86) && defined(WIN32))
+#elif (defined(WEBRTC_ARCH_X86) && defined(_MSC_VER))
 static __inline long int WebRtcIsac_lrint(double x_dbl) {
   long int x_int;
 
```

```
find . -name \*.h -o -name \*.c\* -exec \
    sed 's/Windows.h/windows.h/;s/Mmsystem.h/mmsystem.h/' -i {} \;
env CFLAGS=-D__UCLIBC__ CXXFLAGS=-D__UCLIBC__ meson $PWD $PWD/build \
    --cross-file /path/to/linux-mingw-w64-32bit.txt --default-library static
ninja -C build
DESTDIR=$PWD/install ninja -C build install
```

Building libtgvoip:
```
cmake -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
    -DNoPkgConfig=True -Dopus_INCLUDE_DIRS=$PWD/../../opus-1.3.1/install/usr/local/include/opus \
    -Dwebrtc_audio_CFLAGS="-DWEBRTC_AUDIO_PROCESSING_ONLY_BUILD;-DWEBRTC_WIN;-I$PWD/../../webrtc-audio-processing/install/usr/local/include/webrtc_audio_processing" \
    -DCMAKE_C_FLAGS="-D_WIN32_WINNT=0x0600 -DWINVER=0x0600" \
    -DCMAKE_CXX_FLAGS="-D_WIN32_WINNT=0x0600 -DWINVER=0x0600" ..
make
make install DESTDIR=../install
```


Building the plugin:

```
cmake -DCMAKE_SYSTEM_NAME=Windows \
    -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ \
    -DTd_DIR=/path/to/tdlib/install/usr/local/lib/cmake/Td \
    -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
    -DNoPkgConfig=True \
    -DPurple_INCLUDE_DIRS="$PWD/../../deps/pidgin-2.13.0/libpurple;$PWD/../../deps/win32-dev/gtk_2_0-2.14/include/glib-2.0;$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/glib-2.0/include" \
    -DPurple_LIBRARIES="$PWD/../../deps/pidgin-2.13.0/libpurple/libpurple.dll.a;$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/libglib-2.0.dll.a;$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/libgthread-2.0.dll.a" \
    -Dlibpng_INCLUDE_DIRS=$PWD/../../deps/win32-dev/libpng-1.6.37/install/usr/local/include \
    -Dlibpng_LIBRARIES=$PWD/../../deps/win32-dev/libpng-1.6.37/install/usr/local/lib/libpng16.a \
    -Dlibwebp_INCLUDE_DIRS=$PWD/../../deps/win32-dev/libwebp-1.1.0/install/usr/local/include \
    -Dlibwebp_LIBRARIES=$PWD/../../deps/win32-dev/libwebp-1.1.0/install/usr/local/lib/libwebp.a \
    -DPURPLE_PLUGIN_DIR=/ \
    -DIntl_INCLUDE_DIR=$PWD/../../deps/win32-dev/gtk_2_0-2.14/include \
    -DIntl_LIBRARY=$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/libintl.dll.a \
    -DGLIB_LIBRARIES="$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/libglib-2.0.dll.a;$PWD/../../deps/win32-dev/gtk_2_0-2.14/lib/libgthread-2.0.dll.a" \
    -DSTANDARD_LIBRARIES_EXTRA="-Wl,-Bstatic -lpthread -Wl,-Bdynamic" \
    -Dtgvoip_INCLUDE_DIRS=$PWD/../../deps/win32-dev/libtgvoip/install/usr/local/include/tgvoip \
    -Dtgvoip_LIBRARIES="$PWD/../../deps/win32-dev/libtgvoip/install/usr/local/lib/libtgvoip.a;$PWD/../../deps/win32-dev/opus-1.3.1/install/usr/local/lib/libopus.a;$PWD/../../deps/win32-dev/webrtc-audio-processing/install/usr/local/lib/libwebrtc_audio_processing.a;iphlpapi;winmm" \
    -DCMAKE_BUILD_TYPE=Release ..

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
WINEPATH=$PWD/../../deps/win32-dev/gtk_2_0-2.14/bin wine test/tests
```
