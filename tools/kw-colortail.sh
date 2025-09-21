#!/bin/bash

BINDIR=$(dirname $0)

TAILOPT=-f
PROGDIR=bin/Release
LOGFILE=AyaorihIME
SCRIPT=kanchokuws
LINENUM=
LOGPATH=
LESS_CMD=

while [ "$1" ]; do
    if [ "$1" == "-b" ]; then
        PROGDIR=../bin
        shift
    elif [ "$1" == "-d" ]; then
        PROGDIR=bin/Debug
        shift
    elif [ "$1" == "-u" ]; then
        LOGFILE=kw-uni
        SCRIPT=kw-uni
        shift
    elif [ "$1" == "-n" ]; then
        shift
        if [ "$1" ] && [[ $1 =~ ^[0-9]+ ]]; then
            LINENUM=$1
            shift
        else
            LINENUM=1000
        fi
    else
        if [[ $1 =~ ^[0-9]+ ]]; then
            LINENUM=$1
        else
            LOGPATH=$1
        fi
        shift
    fi
done

if [ "$LINENUM" ]; then
    TAILOPT=-n
    LESS_CMD="| less -R -F -X"
fi

[ $LOGPATH ] || LOGPATH=$PROGDIR/${LOGFILE}.log

CMD="tail $TAILOPT $LINENUM $LOGPATH | $BINDIR/${SCRIPT}_colorcat.sh $LESS_CMD"
echo "$CMD"
eval "$CMD"
