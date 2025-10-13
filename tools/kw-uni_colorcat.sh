#! /bin/bash

BINDIR=$(dirname $0)

cat $* | \
    $BINDIR/colorcat.sh 'DecoderImpl.HandleDeckey.*(ENTER.*,|LEAVE.*,)|\w+::(BEGIN|END|DONE).*|LatticeImpl.addPieces.* (ENTER|LEAVE):.*' | \
    $BINDIR/colorcat.sh -r 'WARN[H:].*' | \
    $BINDIR/colorcat.sh -y ' WARN ' | \
    $BINDIR/colorcat.sh -r ' ERROR ' | \
    $BINDIR/colorcat.sh -c ' INFOH? |ENTER(: deckey=[^,]*, mod)|ENTER_|LEAVE_|CALLED_|==== TEST\([0-9]+\):.* ====' | \
    $BINDIR/colorcat.sh -b 'GetKeyCombinationWhenKeyUp|_findCombo|_isCombinationTiming' | \
    $BINDIR/colorcat.sh -g '[、。ぁ-龠]+'
