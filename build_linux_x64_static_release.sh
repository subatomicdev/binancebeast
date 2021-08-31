#!/bin/sh


echo
echo Preparing vcpkg
echo

cd vcpkg

./bootstrap-vcpkg.sh


echo
echo Installing packages
echo

./vcpkg install boost-beast boost-json gtest --triplet x64-linux


echo
echo Building
echo

cd ../binancebeast

cmake . -DCMAKE_BUILD_TYPE=Release && make


echo
echo Building
echo


echo
echo Running Test
echo 


cd bin
./firstbuildtest


echo
echo Done
echo 

