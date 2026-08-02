#! /bin/bash

cd $(dirname $0)/../..
pwd

TARGET_DIR=publish
if [ "$1" ]; then
  TARGET_DIR=$1
fi

echo "TARGET_DIR=$TARGET_DIR"

for x in $(find tables/{かな系,英字系,漢直系,その他} -type f  -mtime -90); do
  echo cp $x $TARGET_DIR/$x
  cp $x $TARGET_DIR/$x
done
