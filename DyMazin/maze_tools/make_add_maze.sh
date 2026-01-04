#!/bin/bash

. ~/bin/debug_util.sh

MY_DIR=$(dirname $0)
BIN_DIR=$(cd $MY_DIR/../../bin/Release; pwd)
#EXECUTER="ruby $MY_DIR/make_ipa_maze.rb"
EXPANDER="$BIN_DIR/dymaz.exe expand"
COMPILER="$BIN_DIR/dymaz.exe make-dict"
SRC_DIR=$MY_DIR/../work/ipa
#TGT_DIR=/c/Dev/CSharp/KanchokuWS/src/DyMazin/work/ipa
TGT_DIR=$MY_DIR/../work/maze_ipadic

mkdir -p $TGT_DIR/bin

BASENAME=Additional.csv
RUN_CMD -m "$EXPANDER $SRC_DIR/$BASENAME > $TGT_DIR/$BASENAME"

cd $TGT_DIR
RUN_CMD -m "pwd"
RUN_CMD -m "$COMPILER --build-sysdic -o bin -L info"
