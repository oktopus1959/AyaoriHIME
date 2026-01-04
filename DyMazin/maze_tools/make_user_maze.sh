#!/bin/bash

. ~/bin/debug_util.sh

MY_DIR=$(dirname $0)
BIN_DIR=$(cd $MY_DIR/../../bin/Release; pwd)
#EXECUTER="ruby $MY_DIR/make_ipa_maze.rb"
EXPANDER="$BIN_DIR/dymaz.exe expand"
COMPILER="$BIN_DIR/dymaz.exe make-dict"
SRC_DIR=$(cd $MY_DIR/../work/user; pwd)
#TGT_DIR=/c/Dev/CSharp/KanchokuWS/src/DyMazin/work/ipa
TGT_DIR=$(cd $MY_DIR/../work/maze_ipadic/user; pwd)
USER_CSV=User.csv
TGT_CSV=$TGT_DIR/$USER_CSV
USER_DIC=bin/user.dic

mkdir -p $TGT_DIR/bin

rm -f $TGT_CSV
for x in $SRC_DIR/*.csv; do
    RUN_CMD -m "$EXPANDER $x >> $TGT_CSV"
done

RUN_CMD -m "cd $TGT_DIR"
pwd
RUN_CMD -m "$COMPILER --dicdir .. --userdic $USER_DIC -L info ./$USER_CSV"
