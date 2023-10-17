set -e

# Set base variables
progName=`basename $0`
numSubs=4
blockSize=10000
numBlocks=100
numFiles=10
#interface=192.168.58.141
interface=127.0.0.1

# Set derived variables
testDir=/tmp/$progName
pubRepo=$testDir/pubRepo
pubProds=$pubRepo/products
subRepoTempl=$testDir/subRepo

# Clean up any previous session
rm -rf $testDir
mkdir $testDir

declare -a subRepos subProds subPids
iSub=0; while test $((iSub++)) -lt $numSubs; do
    subRepos[$iSub]=${subRepoTempl}_$iSub
    subProds[$iSub]=${subRepos[$iSub]}/products
    mkdir -p ${subProds[$iSub]}
done

# Ensure termination of all child processes on exit
trap wait EXIT
trap "trap '' TERM; kill 0; exit" INT

# Start the publisher
echo "$progName: Starting publisher"
../main/publish/publish -d 50000 -l NOTE -s $interface -r $pubRepo &
sleep 1 # Give the publisher time
pubPid=$!

# Start the subscribers
logLevel=INFO
iSub=0; while test $((iSub++)) -lt $numSubs; do
    echo "$progName: Starting subscriber $iSub"
    ../main/subscribe/subscribe -l $logLevel -r ${subRepos[$iSub]} $interface &
    logLevel=NOTE
    subPids[$iSub]=$!
    sleep 1 # Give the subscriber time
done

# Create and distribute the files
echo "$progName: Creating files"
inotifywait -mq -e moved_to ${subProds[*]} | ( \
    sleep 1 # Give inotifywait(1) time
    iFile=0; while test $((iFile++)) -lt $numFiles; do
        pubFile=$pubProds/$iFile
#       echo "$progName: Creating file $pubFile"
        dd ibs=$blockSize count=$numBlocks </dev/urandom >$pubFile 2>/dev/null
        iSub=0; while test $((iSub++)) -lt $numSubs; do
#           sleep 1
            read line
#           echo $progName: $line
        done
#       echo $progName: File $iFile received
    done
    ps -e -o user,pid,cmd | grep -w inotifywait | awk '$1 ~ /^'$USER'$/ && $3 ~ /^inotifywait$/ {
            system("kill " $2);}'
)

# Verify distribution of the files
iFile=0; while test $((iFile++)) -lt $numFiles; do
    iSub=0; while test $((iSub++)) -lt $numSubs; do
        # Compare file with original
        cmp ${subProds[$iSub]}/$iFile $pubProds/$iFile
    done
done

# Kill the subscribers
#echo "$progName: Killing subscribers"
iSub=0; while test $((iSub++)) -lt $numSubs; do
    pid=${subPids[$iSub]}
    echo "$progName: Killing subscriber $pid"
    kill $pid
done

# Kill the publisher
echo "$progName: Killing publisher $pubPid"
kill $pubPid
