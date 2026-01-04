#!/bin/bash

. ~/bin/debug_util.sh

MY_DIR=$(dirname $0)
BIN_DIR=$(cd $MY_DIR/../../bin/Release; pwd)
#EXECUTER="ruby $MY_DIR/make_ipa_maze.rb"
EXPANDER="$BIN_DIR/dymaz.exe expand"
COMPILER="$BIN_DIR/dymaz.exe make-dict"
SRC_DIR=$MY_DIR/../work/ipa
if [ "$1" ]; then
    if [ "$1" == "-" ]; then
        SRC_FILES=
    else
        while [ "$1" ]; do
            SRC_FILES="$SRC_FILES $SRC_DIR/$1"
            shift
        done
    fi
else
    SRC_FILES=$(echo $SRC_DIR/*.csv)
fi
#TGT_DIR=/c/Dev/CSharp/KanchokuWS/src/DyMazin/work/ipa
TGT_DIR=$MY_DIR/../work/maze_ipadic

mkdir -p $TGT_DIR/bin

#rm -f $TGT_DIR/Maze.csv
if [ "$SRC_FILES" ]; then
    for x in $SRC_FILES; do
        BASENAME=$(basename $x)
        RUN_CMD -m "$EXPANDER $x > $TGT_DIR/$BASENAME"
    done
fi

cd $TGT_DIR
RUN_CMD -m "pwd"
RUN_CMD -m "$COMPILER --build-sysdic -o bin -L info"
