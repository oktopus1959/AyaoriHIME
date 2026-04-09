#! /bin/bash

. ~/bin/debug_util.sh

BUILD_DIR=work/build
MERGED_SRC=$BUILD_DIR/ngram-sys-source.txt

RUN_CMD -m "mkdir -p $BUILD_DIR"
RUN_CMD -m "ruby tools/merge_realtime_ngram_cost.rb --output $MERGED_SRC"
RUN_CMD -m "../bin/Release/ngramer.exe make-dict -L info -d $BUILD_DIR/ -o work/bin/"
