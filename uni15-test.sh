set -e

HOST=uni15-r
TEST_FILE=TestFile

# Create a test file to be distributed
dd ibs=10000 count=1 </dev/urandom >/tmp/$TEST_FILE 2>/dev/null

# Copy the test file to the publisher's host
scp /tmp/$TEST_FILE $HOST:/tmp/$TEST_FILE

ssh -x -T $HOST bash --login <<EOF
    set -e -x

    # Go to the test directory
    cd bicast

    # Clear the publisher's root directory
    rm -rf pubRoot

    # Start the publisher
    if publish -p 128.117.140.125:38801 &>publish.log & then
        PID=\$!
        echo \$PID >publish.pid
        sleep 2 # Allow publish(1) to create directory structure

        # Link to the test file
        if ! ln -s /tmp/$TEST_FILE pubRoot/products/$TEST_FILE; then
            kill \$PID
            wait \$PID
            exit 1
        fi
    fi
EOF

sleep 1

ssh -x -T $HOST bash --login <<\EOF
    set -e -x

    # Go to the test directory
    cd bicast

    # Terminate the publisher
    PID=`cat publish.pid`
    kill $PID
    # Can't `wait $PID` because it's not a child of this shell
EOF