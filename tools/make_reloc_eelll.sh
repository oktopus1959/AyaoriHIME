#! /bin/bash

. ~/bin/debug_util.sh

SRCDIR=$(dirname $0)/../..

UNK_CHARS="岳災快包暮陣繁紹酸尚舎盤攻略貿汽揺典桜房紀頻秒犯又亘晶卑礎衡竜丹繊妙己絶束罪老阿季雲窓植矢献里寝貨看捕湯香浦秘益温測枚均団界彦須午柄慶潟樹杉岸援欧句笑肉姿節密弁丘盛招賀則障干郷寿幹貸貧償震源端痛革願群舗豆稲奈夕卸鬼魂殊鶴鹿障催貴若麻亜塩整"

RUN_CMD -m "tail -n 1000 $SRCDIR/eelll.txt | \
    grep '[$UNK_CHARS]' ../eelll.txt  | \
    ruby tools/extract-lines-with-all-kanji.rb | \
    ruby -e 'while line=gets; if line =~ /・.*・.*・/; line = line.gsub(\"・\", \"、\"); end; puts line; end' > $SRCDIR/eelll.reloc.txt"
