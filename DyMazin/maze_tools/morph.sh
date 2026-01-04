#! /bin/bash

. ~/bin/debug_util.sh

#IGNORE_EOS=--ignore-eos

RUN_CMD -m "cat | ../bin/Release/dymaz.exe -d work/maze_ipadic/bin --userdic work/maze_ipadic/user/bin/user.dic  -L info -F'%m\t%H ||| %phl,%phr,%pb,%pw,%pC,%pn,%pc\n' --eos-format='EOS,%phl,%phr,%pb,%pw,%pC,%pn,%pc' --non-terminal-cost=5000 -N3 --maze-penalty 1000 --maze-conn-penalty 3000 $*"
