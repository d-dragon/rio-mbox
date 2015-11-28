#!/bin/bash

CUR_DIR=`pwd`
WP="wiringPi-d795066"

if [ ! -d "lib/$WP" ]; then
    cd lib
    tar -xvf $WP.tar.gz
    cd $WP
    ./build
    cd $CUR_DIR	
fi

sudo pkill startMe
sudo pkill Mbox

make clean
make
sudo make install
