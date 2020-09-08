#!/bin/sh

rm -f $1
for lang in `cat $(dirname $0)/LINGUAS`; do
    echo File /nonfatal \"/oname=$lang\\LC_MESSAGES\\telegram-purple.mo\" \"\${BUILD_DIR}\\$lang.gmo\" >>$1
done
