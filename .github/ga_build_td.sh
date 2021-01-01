#!/bin/sh

set -ex
cd "$(dirname $0)/.."

# == CONFIGURATION ==
if [ "$#" != "2" ] ; then
    echo "USAGE: $0 TAG MARK"
    exit 1
fi
TD_TAG="$1"
TD_MARK="$2"
# Just in case we want to change it.
CACHE_DIR=".ga_cache"
TARFILE="${CACHE_DIR}/td_destdir_${TD_TAG}_${TD_MARK}.tar.lzop"

# == CHECK CACHE ==
if [ -r "${TARFILE}" ]; then
    echo "Using existing td_destdir for tag ${TD_TAG}, mark ${TD_MARK}."
    rm -rf td_destdir # Just in case
    tar xaf "${TARFILE}"
    # TODO: If GitHub Actions messes up the file, we have to increment 'mark'.
    echo "Great success!"
    exit 0
fi

# == BUILD TDLIB ==
mkdir -p "${CACHE_DIR}"
rm -rf td_repo # Just in case
git clone -q -c advice.detachedHead=false -b "${TD_TAG}" --depth 1 https://github.com/tdlib/td.git td_repo
cd td_repo
    mkdir build
    cd build
        cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release
        DESTDIR="../../td_destdir" ninja install
    cd ..
cd ..

# == CREATE CACHE ==
tar caf "${TARFILE}" "td_destdir"

# Now you can build tdlib-purple using
# `cmake -DTd_DIR=../td_destdir/usr/local/lib/cmake/Td ..`
