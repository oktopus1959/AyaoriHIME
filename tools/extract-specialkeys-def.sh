#!/bin/bash

grep '   new KeyOrFunction.KeyOrFunction.ATTR_FUNCTION, ' AyaoriHIME/Domain/SpecialKeysAndFunctions.cs \
    | cut -d, -f4,6-10 \
    | sed -r 's/^ *"/|/' \
    | sed -r 's/", "/|/' \
    | sed -r 's/"\),.*$/|/' \
    | ruby -e 'n=0; while line=gets; puts "|#{n}#{line}"; n+=1; end'
