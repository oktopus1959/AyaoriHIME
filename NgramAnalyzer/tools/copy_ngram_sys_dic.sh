#/bin/bash

. ~/bin/debug_util.sh

RUN_CMD "cp work/bin/ngram-sys.dic /f/Dev/CSharp/AyaoriHIME/systemFiles/ngram/dic/ngram-sys.dic"
RUN_CMD "cp work/bin/ngram-sys.dic /f/Dev/CSharp/AyaoriHIME/publish/systemFiles/ngram/dic/ngram-sys.dic"
RUN_CMD "cp work/bin/char-3gram.bin /f/Dev/CSharp/AyaoriHIME/systemFiles/ngram/dic/char-3gram.bin"
RUN_CMD "cp work/bin/char-3gram.bin /f/Dev/CSharp/AyaoriHIME/publish/systemFiles/ngram/dic/char-3gram.bin"
