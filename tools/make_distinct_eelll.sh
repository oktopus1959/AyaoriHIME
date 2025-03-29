#! /bin/bash

. ~/bin/debug_util.sh

SRCDIR=$(dirname $0)/../..

RUN_CMD -m "tail -n 1000 $SRCDIR/eelll.txt | ruby tools/extract-lines-with-all-kanji.rb > $SRCDIR/eelll.distinct.txt"
