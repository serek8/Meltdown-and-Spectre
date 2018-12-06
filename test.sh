#!/bin/bash

set -e
make clean all >/dev/null

function newsecret()
{
    secret=`head -c 100 </dev/urandom | md5sum | awk '{ print $1 }'`
    if [ `echo -n $secret | wc -c` != 32 ]
    then
        echo Expected secret to be 32 bytes.
        exit 1
    fi
    echo "Generated secret: $secret" >&2
    echo $secret >/dev/wom
}

# THRESHOLD and N can be exported from the parent shell

if [ "$THRESHOLD" = "" ]
then THRESHOLD=100
fi

if [ "$N" = "" ]
then N=100
fi

if [ "$VUNETID" = "" ]
then    
    echo "Please export your vu-net-id using:"
    echo ' $ export VUNETID=bgs137'
    exit 0
fi

MELTDOWN=${VUNETID}-meltdown
SPECTRE=${VUNETID}-spectre

if [ "$1" = "batch" ]
then
    for q in 1 2
    do
      for bin in $MELTDOWN $SPECTRE
      do
        if [ ! -e $bin ]
        then    echo "No $bin found"
                continue
        fi
        if [ "$THRESHOLD" = "" ]
        then
            echo "Reliability of $N $bin with autotuned threshold.."
        else
            if [ $bin = "$SPECTRE" ]
            then
                continue
            fi
            echo "Reliability of $N $bin with given threshold of $THRESHOLD.."
        fi

        newsecret
        rm -f log
        for x in `seq 1 $N`
        do  
            rm -f out
            if [ "$THRESHOLD" = "" ]
            then
                ./$bin >out 2>>errlog
            else
                ./$bin $THRESHOLD >out 2>>errlog
            fi
            grep -q $secret out && echo -n "." || echo -n "?"
            cat out >>log
        done
        echo ''
        echo -n 'Success rate: '
        grep -c $secret log  || true
        echo ''
      done
    
      unset THRESHOLD
    done

    exit 0
fi

# Single mode

for q in 1 2
do
  for bin in $MELTDOWN $SPECTRE
  do
      if [ "$THRESHOLD" = "" ]
      then
          echo " * $bin - autotuned threshold"
          newsecret
          ./$bin >out
      else
          if [ $bin = "$SPECTRE" ]
          then
             continue
          fi
          echo " *  $bin - given threshold of $THRESHOLD"
          newsecret
          ./$bin $THRESHOLD >out
      fi
      cat out
      grep -q $secret out && echo pass || echo fail
      echo ''
  done
  unset THRESHOLD
done
