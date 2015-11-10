#!/bin/bash

sudo pkill startMe
sudo pkill Mbox

make clean
make
sudo make install
