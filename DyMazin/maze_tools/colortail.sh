#! /bin/bash

. ~/bin/debug_util.sh

TAILOPT="-n 100"
if [ "$1" ]; then
    TAILOPT="-n $1"
fi

RUN_CMD -m "tail $TAILOPT ../bin/Release/dymaz.log | ~/bin/colorcat.sh 'main::|origKey|mazeKey|hiraYomi|kanji[1234]=.*' | ~/bin/colorcat.sh -r 'put |after find_yomi|yomis=' | ~/bin/colorcat.sh -g 'make_variation|findYomi.*(Matched|stage-.)|main.*ENTER.*' | less"
