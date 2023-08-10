set -e

rm -rf pubrepo subrepo_*

#numSubs=4
numSubs=2
blockSize=1440
#numBlocks=2
numBlocks=1
#numFiles=2
numFiles=1
interFileUsleep=100000
interface=192.168.58.132 
#interface=127.0.0.1

# Start publisher
echo Starting publisher
../main/publish/publish -s $interface -r pubrepo &
pubPid=$!

sleep 1

# Start subscribers
declare -a subPids
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Starting subscriber $iSub
    ../main/subscribe/subscribe -r subrepo_$iSub $interface &
    subPids[$iSub]=$!
done

sleep $numSubs

# Create files to distribute
iFile=0; while test $((iFile++)) -lt $numFiles; do
    echo Creating file $iFile
    dd ibs=$blockSize count=$numBlocks </dev/urandom >pubrepo/$iFile || exit 1
    usleep $interFileUsleep
done

# Wait for files to show up in the subscriber's repositories
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Testing for files received by subscriber $iSub
    #while test `ls subrepo_$iSub/ | wc -w 2>/dev/null` -lt $numFiles; do
    #    usleep $interFileUsleep
    #done
    test `ls subrepo_$iSub/ | wc -w 2>/dev/null` -lt $numFiles || sleep 1
done

# Compare subscriber repositories with publisher's
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Comparing files received by subscriber $iSub
    iFile=0; while test $((iFile++)) -lt $numFiles; do
        cmp subrepo_$iSub/$iFile pubrepo/$iFile
    done
done

iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Killing subscriber $iSub
    kill ${subPids[$iSub]}
done
echo Killing publisher
kill $pubPid
wait
