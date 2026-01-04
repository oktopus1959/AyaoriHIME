#! /bin/bash

. ~/bin/debug_util.sh

WIKI=wikipedia-20210708/_all/wikipedia.all.txt
HPLT=HPLT/excerpt/hplt.except.txt

OPTS=
#ALPHA=0.5
#BETA=0.5
ALPHA=0.9
BETA=0.1
MAX_SCORE=10000

while [ "$1" ]; do
    case "$1" in
        --alpha) shift; ALPHA="$1";;
        --beta) shift; BETA="$1";;
        --maxS) shift; MAX_SCORE="$1";;
        --*) echo "Unknown opt: $1" >&2; exit;;
        *) break;;
    esac
    shift
done

OPTS="--alpha $ALPHA --beta $BETA --maxS $MAX_SCORE"

if [ "$1" == "-" ]; then
    SRC_FILE=
elif [ "$1" ]; then
    SRC_FILE="$1"
else
    SRC_FILE="input/wiki_hplt.all.txt"
fi
SUFFIX=${ALPHA}-${BETA}-${MAX_SCORE}
OUTFILE=wiki_hplt.all.score.full.${SUFFIX}.txt
SCOREFILE=wiki_hplt.all.score.${SUFFIX}.txt
RUN_CMD -m "mkdir -p output"
RUN_CMD -m "ruby /f/Dev/Text/tools/calc_score.rb $OPTS $SRC_FILE | tail -n +2 | sort -n -k7 > output/$OUTFILE"
RUN_CMD -m "mv -f wiki_hplt.all.score*.txt junk/"
RUN_CMD -m "cut -d$'\t' -f1,7 output/$OUTFILE > $SCOREFILE"
