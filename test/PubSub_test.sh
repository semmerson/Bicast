set -ex

rm -rf pubrepo subrepo

# Start publisher
../main/publisher/publisher -l DEBUG -P 127.0.0.1:38800 -p 127.0.0.1 -r pubrepo -S 127.0.0.1  &
pubPid=$!

sleep 1

# Start subscriber
../main/subscriber/subscriber -l DEBUG -r subrepo 127.0.0.1:38800 127.0.0.1 &
subPid=$!

sleep 1

# Create files to distribute
if dd count=10 </dev/urandom >pubrepo/1 && dd count=20 </dev/urandom >pubrepo/2; then
    # Wait
    sleep 1

    # Compare subscriber's repository with publisher's
    cmp subrepo/1 pubrepo/1
fi

kill $subPid $pubPid
