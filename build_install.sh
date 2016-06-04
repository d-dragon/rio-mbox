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
#sudo make install
if [ 0 -eq $? ]; then
    tar -czf firmware.tar.gz bin/Mbox files/startMediaHub.sh files/omxplayer_dbus_control.sh src/ftplib_example.py files/config.txt files/turnonoff_tv.sh files/initmbox
	
fi
