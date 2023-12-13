# Create test files
# @param[in] numFiles   Number of file to create
# @param[in] dir        Pathname of directory for the files. File pathnames will be $dir/file-$i,
#                       where $i goes from 1 to $numFiles
createTestFiles() {
    numFiles=${1:?Number of files missing}
    dir=${2:?Directory missing}

    mkdir -p $dir
    i=0; while test $((i++)) -lt $numFiles; do
        dd ibs=100000 count=1 </dev/urandom >$dir/file-$i 2>/dev/null
    done
}

# Copy files to the remote host
# @param[in] srcDir  Pathname of directory containing the file to be copied
# @param[in] dstDir  Pathname of directory on the remote host to copy the files
copyFilesToRemote() {
    srcDir=${1:?Source directory missing}
    dstDir=${2:?Destination directory missing}
    scp -r -q $srcDir $remoteHost:$dstDir
}

# Create test files and copy to the remote host
# @param[in] $1  Number of file to create
# @param[in] $2  Pathname of local directory for the files. File pathnames will be $dir/file-$i,
#                where $i goes from 1 to $numFiles
# @param[in] $3  Pathname of directory on the remote host to copy the files
createFiles() {
    createTestFiles $1 $2
    copyFilesToRemote $2 $3
}

# Execute the publisher
startPublisher() {
    ssh -x -T $remoteHost bash --login <<EOF
        set -e

        # Go to the relevant directory
        cd bicast

        # Execute the publisher as a background process
        echo >publish.log
        publish -l INFO -P $remoteAddr -p $remoteAddr:38801 &>>publish.log &
        echo \$! >publish.pid
EOF
}

# Watch for received files in a background process
watch() {
    (inotifywait --monitor --recursive --quiet --event moved_to -- subRoot/products | (
        set -e
        i=0; while test $i -lt $(($numOld+$numNew)); do
            # Wait for a file to arrive
            read -r dir event filename
            relPathname=$dir$filename
            if test -f $relPathname; then
                prodName=`echo $relPathname | sed "s:subRoot/products/::"`
                echo File arrived: \"$prodName\"
                cmp $tmpDir/$prodName $relPathname
                i=$((++i))
            fi
        done

        ps -o comm,pid | awk '/^inotifywait/ {system("kill " $2); exit}'
    )) &
}

# Terminate the publisher
stopPublisher() {
    ssh -x -T $remoteHost bash --login <<EOF
        set -e

        # Go to the relevant directory
        cd bicast

        # Terminate the publisher
        PID=\$(cat publish.pid)
        kill \$PID
        # Can't `wait $PID` because it's not a child of this shell
EOF
}

####################################################################################################
#
# Begin program
#
####################################################################################################

set -e

cd `dirname $0`

# Basic variables
remoteHost=uni15-r
remoteAddr=128.117.140.125
tmpDir=/tmp/bicast
numOld=5
numNew=5

# Initialize the subscriber's environment
rm -rf $tmpDir
mkdir -p $tmpDir
rm -rf subRoot
mkdir -p subRoot/products/{old,new}

# Initialize the publisher's environment
ssh -x -T $remoteHost bash --login <<EOF
    rm -rf bicast/pubRoot
    mkdir -p bicast/pubRoot/products
EOF

# Create backlog files and copy them to the publisher
createFiles $numOld $tmpDir/old bicast/pubRoot/products

# Execute the publisher
startPublisher
trap stopPublisher EXIT

# Create new files and copy them to the publisher
createFiles $numNew $tmpDir/new bicast/pubRoot/products

# Watch for received files in a background process
watch
watchPid=$!

# Execute the subscriber in a background process
subscribe -l INFO $remoteAddr &>subscribe.log &
subscribePid=$!

# Wait for the watching process to terminate
wait $watchPid

# Terminate the subscriber process
kill $subscribePid && wait $subscribePid