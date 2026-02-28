#! /bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "../bin/Release/ngramer.exe make-dict -L info -d work/ -o work/bin/"
