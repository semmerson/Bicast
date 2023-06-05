set -ex

rm -rf pubrepo subrepo_*

numSubs=2
blockSize=1440
numBlocks=1
numFiles=1

# Start publisher
echo Starting publisher
../main/publish/publish -l DEBUG -P 127.0.0.1:38800 -p 127.0.0.1 -r pubrepo -S 127.0.0.1  &
pubPid=$!

sleep 1

# Start subscribers
declare -a subPids
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Starting subscriber $iSub
    ../main/subscribe/subscribe -l DEBUG -r subrepo_$iSub 127.0.0.1:38800 127.0.0.1 &
    subPids[$iSub]=$!
done

sleep $numSubs

# Create files to distribute
iFile=0; while test $((iFile++)) -lt $numFiles; do
    echo Creating file $iFile
    dd ibs=$blockSize count=$numBlocks </dev/urandom >pubrepo/$iFile || exit 1
done

# Wait for files to show up in the subscriber's repositories
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Testing for files received by subscriber $iSub
    test `ls subrepo_$iSub/ | wc -w 2>/dev/null` -lt $numFiles || sleep 1
done

# Compare subscriber repositories with publisher's
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo Comparing files received by subscriber $iSub
    iFile=0; while test $((iFile++)) -lt $numFiles; do
        cmp subrepo_$iSub/$iFile pubrepo/$iFile
    done
done

for subPid in ${subPids[*]}; do
    echo Killing subscriber $iSub
    kill $subPid
done
echo Killing publisher
kill $pubPid
wait
