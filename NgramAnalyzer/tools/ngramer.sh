#! /bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "cat | ../bin/Release/ngramer.exe -L debug -d work/bin/ -N3"
