This repo is source code of Mbox which is a part of project Rio. The application will run on Raspberry Pi 2
https://bitbucket.org/d-dragon/rio_mbox

1. Dependencies
Project use some thirdparty libraries: libxml2, libconfig, libpython2.7, wiringPi

2. Build and install application
Clone source code at: https://bitbucket.org/d-dragon/rio_mbox
Navigate to root directory of source folder
$ ./build_install.sh
$ sudo reboot

3. Configuration
Modify station address in file /etc/mbox.cfg
Then restart board

4. Debug
Program log was located at /var/log/user.log

Owner: Duy Phan
