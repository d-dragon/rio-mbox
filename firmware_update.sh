FIRMWARE_DIR="/tmp/mbox_firmware"
FIRMWARE_NAME="firmware.tar.gz"
CUR_DIR=`pwd`

if [ ! -d $FIRMWARE_DIR ]; then
mkdir $FIRMWARE_DIR
fi

#download firmware...
wget $1 -O $FIRMWARE_DIR/$FIRMWARE_NAME

cd $FIRMWARE_DIR
tar xvf $FIRMWARE_NAME 

sudo pkill startMediaHub.sh
sudo pkill Mbox

cp bin/Mbox /usr/bin/
cp files/startMediaHub.sh /usr/bin/
cp files/omxplayer_dbus_control.sh /usr/bin/
cp src/ftplib_example.py /usr/bin/
cp files/config.txt /boot/
cp files/turnonoff_tv.sh /usr/bin/
cp files/initmbox /etc/init.d/

cd $CUR_FIR
rm -rf $FIRMWARE_DIR

