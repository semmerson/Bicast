libdir=$1?Library directory not specified}
cd /usr/src/gtest
sudo cmake CMakeLists.txt
sudo make
sudo cp *.a $libdir