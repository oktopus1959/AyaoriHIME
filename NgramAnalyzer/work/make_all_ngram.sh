#! /bin/bash

. ~/bin/debug_util.sh

SRCDIR=/f/Dev/Text/output
OUTFILE=input/wiki_hplt.all.txt

RUN_CMD -m "mv -f $OUTFILE junk/"

RUN_CMD -m "cat $SRCDIR/wiki_hplt.hiragana.[12345]gram.txt > $OUTFILE"
for x in 1 2 3 4; do
    RUN_CMD -m "sort -rn -k2 $SRCDIR/wiki_hplt.kanji.${x}gram.txt | head -n 300000 >> $OUTFILE"
done
for x in 2 3 4 5; do
    RUN_CMD -m "sort -rn -k2 $SRCDIR/wiki_hplt.hirakan.${x}gram.txt | head -n 300000 >> $OUTFILE"
done

