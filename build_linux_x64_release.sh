#!/bin/sh


echo
echo Preparing vcpkg
echo

cd vcpkg

./bootstrap-vcpkg.sh


echo
echo Installing packages
echo

./vcpkg install boost-beast boost-json --triplet x64-linux


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

pwd 

cd bin
./bbtest postbuildtest


echo
echo Done
echo 

