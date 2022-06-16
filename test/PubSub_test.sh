set -ex

srcDir=`basename $0`

publisher/publisher -l DEBUG -P localhost:38800 -p localhost -r pubrepo -S localhost  &
subscriber/subscriber -l DEBUG -r subrepo localhost:38800 localhost &

#ln $srcDir/publisher pubrepo/publisher