#!/bin/sh

rm -f $1
for lang in `cat $(dirname $0)/LINGUAS`; do
    echo SetOutPath \"\$PidginDir\\locale\\$lang\\LC_MESSAGES\" >>$1
    echo File /nonfatal \"/oname=\${PRODUCT_NAME}.mo\" \"\${BUILD_DIR}\\$lang.gmo\" >>$1
done
