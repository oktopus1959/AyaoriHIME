#! /bin/bash

. ~/bin/debug_util.sh

RUN_CMD -m "cat | ../bin/Release/dymaz.exe -d work/maze_ipadic/bin --userdic work/maze_ipadic/user/bin/user.dic  -L info -F'%m\t%H ||| %phl,%phr,%pb,w:%pw,c:%pC,a:%pc\n' --eos-format='EOS,%phl,%phr,%pb,%pw,%pC,%pn,%pc' --non-terminal-cost=5000 -N5 --maze-penalty -1 --maze-conn-penalty 3000 $*"
