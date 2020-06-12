tdlib:
```
sed s/Crypt32/crypt32/ -i tdnet/CMakeLists.txt
sed 's/Mswsock/mswsock/;s/Normaliz/normaliz/' -i tdutils/CMakeLists.txt
sed 's/WinSock2.h/winsock2.h/;s/WS2tcpip.h/ws2tcpip.h/;s/MSWSock.h/mswsock.h/;s/Windows.h/windows.h/' -i tdutils/td/utils/common.h
```
replace `if(WIN32)` with `if(CMAKE_HOST_WIN32)` for GIT_COMMIT_CMD command
move `target_link_libraries(tdutils PRIVATE ws2_32 mswsock normaliz)` towards the end of tdutil/CMakeLists.txt
```
cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DOPENSSL_ROOT_DIR=/usr/i686-w64-mingw32/sys-root/mingw -DOPENSSL_USE_STATIC_LIBS=TRUE -DZLIB_FOUND=1 -DZLIB_LIBRARIES=/usr/i686-w64-mingw32/sys-root/mingw/lib/libz.a -DCMAKE_BUILD_TYPE=Release ..
```

This plugin:
```
cmake -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=i686-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=i686-w64-mingw32-g++ -DPKG_CONFIG_EXECUTABLE=/usr/bin/i686-w64-mingw32-pkg-config -DTd_DIR=/path/to/tdlib/usr/local/lib/cmake/Td -DCMAKE_SHARED_LINKER_FLAGS="-static-libgcc -static-libstdc++" -DCMAKE_BUILD_TYPE=Release ..
```

libpurple:

patch
```
libtoolize --force --copy --install
autoreconf -f -i
./configure --host i686-w64-mingw32 --target i686-w64-mingw32 --disable-gtkui --disable-gstreamer --disable-vv --disable-meanwhile --disable-avahi --disable-dbus --disable-perl --disable-nss --disable-gnutls --without-x; sed /socklen_t/d -i config.h
```

TBC
