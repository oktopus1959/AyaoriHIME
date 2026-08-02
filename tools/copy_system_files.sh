#! /bin/bash

cd $(dirname $0)/../..
pwd

for x in \
systemFiles/alt-kanji.txt \
systemFiles/dymazin/dic/mazedic/char.bin \
systemFiles/dymazin/dic/mazedic/conn-3gram.bin \
systemFiles/dymazin/dic/mazedic/dicrc \
systemFiles/dymazin/dic/mazedic/joyo-kanji-plus.txt \
systemFiles/dymazin/dic/mazedic/kanji-bigram.txt \
systemFiles/dymazin/dic/mazedic/matrix.bin \
systemFiles/dymazin/dic/mazedic/sys.dic \
systemFiles/dymazin/dic/mazedic/unk.dic \
systemFiles/dymazin/etc/morphrc \
systemFiles/easy_chars.sample.txt \
systemFiles/kanji-yomi.txt \
systemFiles/kwbushu.rev \
systemFiles/kwroman.def.txt \
systemFiles/ngram/dic/char-3gram.bin \
systemFiles/ngram/dic/char-4gram.bin \
systemFiles/ngram/dic/ngram-sys.dic \
systemFiles/simpleDic.sample.txt \
systemFiles/stroke-help.sample.txt \
systemFiles/system.romanDic.txt \
systemFiles/userDic.sample.csv \
; do
  echo cp $x publish/$x
  cp $x publish/$x
done
