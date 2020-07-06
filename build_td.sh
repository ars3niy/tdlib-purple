#!/bin/sh

set -ex
cd "$(dirname $0)"

# == CONFIGURATION ==
# The tag/version of tdlib that we want to check out.
TAG="v1.6.0"
# Change this if you want to invalidate the Travis cache and force a rebuild.
MARK="1"
# Just in case we want to change it.
CACHE_DIR=".travis_cache"

# == SETUP ==
CACHE_ID="${TAG}_m${MARK}"
mkdir -p "${CACHE_DIR}"
mkdir -p build

# == CHECK CACHE ==
echo "${CACHE_ID}" > "${CACHE_DIR}/cache_id_expected.txt"
rm -rf td_destdir # Just in case
if [ -r "${CACHE_DIR}/cache_id_actual.txt" ] && [ -x "${CACHE_DIR}/td_destdir" ] && diff -q "${CACHE_DIR}/cache_id_actual.txt" "${CACHE_DIR}/cache_id_expected.txt"
then
    rm "${CACHE_DIR}/cache_id_expected.txt"
    echo "Using existing td_destdir for tag ${TAG}, mark ${MARK}."
    echo "Great success!"
    exit 0
else
    echo "Cache not usable:"
    echo "    expected: ${CACHE_ID}"
    echo "    actual: $(cat "${CACHE_DIR}/cache_id_actual.txt" 2> /dev/null || echo NONE)"
    rm -rf "${CACHE_DIR}/cache_id_actual.txt" "${CACHE_DIR}/td_destdir"
fi

# == BUILD TDLIB ==
cd build
    rm -rf td_repo # Just in case
    git clone -q -c advice.detachedHead=false -b "${TAG}" --depth 1 https://github.com/tdlib/td.git td_repo
    cd td_repo
        mkdir build
        cd build
            cmake .. -DCMAKE_BUILD_TYPE=Release
            make install -j2 DESTDIR="../../../${CACHE_DIR}/td_destdir"
        cd ..
    cd ..
cd ..

# == CREATE CACHE ==
mv "${CACHE_DIR}/cache_id_expected.txt" "${CACHE_DIR}/cache_id_actual.txt"

# Now you can build tdlib-purple using
# `cmake -DTd_DIR=.travis_cache/td_destdir/usr/local/lib/cmake/Td ..`
