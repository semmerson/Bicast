set -e

HOST=uni15-r
SUBDIR=bicast
RPM=@CMAKE_PROJECT_NAME@-@CMAKE_PROJECT_VERSION@-Linux.rpm 

cd `dirname $0`

# Copy distribution
scp $RPM $HOST:/tmp

# Install distribution -- replacing any existing installation
ssh -x -T root@uni15-r bash --login <<EOF
    su -c "rpm --nodeps -Uvh --replacefiles --replacepkgs -p /tmp/$RPM"
EOF