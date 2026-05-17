#! /bin/bash

. ~/bin/debug_util.sh

BASEDIR=$(cd $(dirname $0)/..; pwd)
BINDIR=$(cd $BASEDIR/../bin/Release; pwd)

RUN_CMD -m "$BINDIR/dymaz.exe -d $BASEDIR/work/maze_ipadic/bin -F'%phl\n%phr,' --bos-format='0,' --eos-format='0' $1"
