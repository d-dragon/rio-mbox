#!/bin/sh
if [ "$1" = "on" ]; then
	echo "turn on tv"
	echo on 0 | cec-client -s
else
	echo "turn off tv"
	echo standby 0 | cec-client -s
fi

