#!/bin/bash 
#get network interface ip
flag=0
echo $1
count=0
while true; do 
#ip=`ifconfig | grep -A 1 $1 | tail -1 | cut -d ':' -f 2 | cut -d ' ' -f 1`
ip=`ifconfig eth0 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
gw=`route -n | grep 'UG[ \t]' | awk '{print $2}'`
mac_addr=`cat /sys/class/net/eth0/address | sed 's/\://g' | tr 'a-z' 'A-Z'`
echo "ip=$ip" >> /var/log/user.log
echo "netmask=$mask" >> /var/log/user.log
echo "gateway=$gw" >> /var/log/user.log

# -q quiet
# -c nb of pings to perform
ping -c 2 $gw > /dev/null

if [ $? -eq 0 ]
then
	echo "network is ok" >> /var/log/user.log
	flag=1
else
	count=`expr $count + 1`
	echo "count=$count" >> /var/log/user.log
	echo "network is not available" >> /var/log/user.log
	sleep 5
	if [ $count -eq 5 ]
	then
		echo "start app failed -> reboot" >> /var/log/user.log
		reboot
	fi
fi
if [ $flag -eq 1 ]
then
	count=0
	if [ -f /etc/mbox.cfg ]
	then
		Mbox
	else
		Mbox $mac_addr
	fi
	wait 
	echo "started Mbox" > /dev/ttyAMA0
	killall Mbox
	flag=0
fi
done

