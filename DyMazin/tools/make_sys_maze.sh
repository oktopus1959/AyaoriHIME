#!/bin/bash

. ~/bin/debug_util.sh

CUR_DIR=$(pwd)
MY_DIR=$(dirname $0)
BIN_DIR=$(cd $MY_DIR/../../bin/Release; pwd | ruby -ne "puts \$_.sub('$CUR_DIR', '.')")
SYSDIC_DIR=$(cd $MY_DIR/../../../systemFiles/dymazin/dic/mazedic; pwd | ruby -ne "puts \$_.sub('$CUR_DIR', '.')")
EXPANDER="$BIN_DIR/dymaz.exe expand"
COMPILER="$BIN_DIR/dymaz.exe make-dict"
SRC_DIR=$(cd $MY_DIR/../work/ipa; pwd | ruby -ne "puts \$_.sub('$CUR_DIR', '.')")
if [ "$1" ]; then
    if [ "$1" == "-" ]; then
        SRC_FILES=
    else
        while [ "$1" ]; do
            SRC_FILES="$SRC_FILES $SRC_DIR/$(basename $1)"
            shift
        done
    fi
else
    SRC_FILES=$(echo $SRC_DIR/*.csv)
fi

TGT_DIR=$(cd $MY_DIR/../work/maze_ipadic; pwd | ruby -ne "puts \$_.sub('$CUR_DIR', '.')")

mkdir -p $TGT_DIR/bin

RUN_CMD -m "rm -f $SRC_DIR/skipped_lines.txt"

if [ "$SRC_FILES" ]; then
    for x in $SRC_FILES; do
        BASENAME=$(basename $x)
        if [ "$BASENAME" != "matrix.def.csv" ]; then
            RUN_CMD -m "$EXPANDER $x > $TGT_DIR/$BASENAME 2>> $SRC_DIR/skipped_lines.txt"
        fi
    done
fi

cd $TGT_DIR
RUN_CMD -m "pwd"
RUN_CMD -m "rm -f matrix.def.csv"
RUN_CMD -m "$COMPILER --build-sysdic -o bin -L info"
RUN_CMD -m "cp bin/sys.dic $SYSDIC_DIR/"
