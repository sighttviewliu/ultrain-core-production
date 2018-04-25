#!/bin/bash
#
# ultrainio-tn_roll is used to have all of the instances of the ULTRAIN daemon on a host brought down
# so that the underlying executable image file (the "text file") can be replaced. Then
# all instances are restarted.
# usage: ultrainio-tn_roll.sh [arglist]
# arglist will be passed to the node's command line. First with no modifiers
# then with --replay and then a third time with --resync
#
# The data directory and log file are set by this script. Do not pass them on
# the command line.
#
# In most cases, simply running ./ultrainio-tn_roll.sh is sufficient.
#

if [ -z "$ULTRAINIO_HOME" ]; then
    echo ULTRAINIO_HOME not set - $0 unable to proceed.
    exit -1
fi

cd $ULTRAINIO_HOME

if [ -z "$ULTRAINIO_NODE" ]; then
    DD=`ls -d var/lib/node_[012]?`
    ddcount=`echo $DD | wc -w`
    if [ $ddcount -gt 1 ]; then
        DD="all"
    fi
    OFS=$((${#DD}-2))
    export ULTRAINIO_NODE=${DD:$OFS}
else
    DD=var/lib/node_$ULTRAINIO_NODE
    if [ ! \( -d $DD \) ]; then
        echo no directory named $PWD/$DD
        cd -
        exit -1
    fi
fi

prog=""
RD=""
for p in ultraind ultrainiod nodultrain; do
    prog=$p
    RD=bin
    if [ -f $RD/$prog ]; then
        break;
    else
        RD=programs/$prog
        if [ -f $RD/$prog ]; then
            break;
        fi
    fi
    prog=""
    RD=""
done

if [ \( -z "$prog" \) -o \( -z "$RD" \) ]; then
    echo unable to locate binary for ultraind or ultrainiod or nodultrain
    exit 1
fi

SDIR=staging/ultrain
if [ ! -e $SDIR/$RD/$prog ]; then
    echo $SDIR/$RD/$prog does not exist
    exit 1
fi

if [ -e $RD/$prog ]; then
    s1=`md5sum $RD/$prog | sed "s/ .*$//"`
    s2=`md5sum $SDIR/$RD/$prog | sed "s/ .*$//"`
    if [ "$s1" == "$s2" ]; then
        echo $HOSTNAME no update $SDIR/$RD/$prog
        exit 1;
    fi
fi

echo DD = $DD

bash $ULTRAINIO_HOME/scripts/ultrainio-tn_down.sh

cp $SDIR/$RD/$prog $RD/$prog

if [ $DD = "all" ]; then
    for ULTRAINIO_RESTART_DATA_DIR in `ls -d var/lib/node_??`; do
        bash $ULTRAINIO_HOME/scripts/ultrainio-tn_up.sh $*
    done
else
    bash $ULTRAINIO_HOME/scripts/ultrainio-tn_up.sh $*
fi
unset ULTRAINIO_RESTART_DATA_DIR

cd -
