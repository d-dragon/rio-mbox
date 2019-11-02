This repo is source code of Mbox which is a part of project Rio. The application will run on Raspberry Pi 2
https://bitbucket.org/d-dragon/rio_mbox.

# Setup, build and configuration

1. Dependencies
Project use some thirdparty libraries: libxml2, libconfig, libpython2.7, wiringPi, libcurl.

2. Build and install application
Clone source code at: https://bitbucket.org/d-dragon/rio_mbox
Navigate to root directory of source folder.
```
$ ./build_install.sh
$ sudo reboot
```

3. Configuration
Modify station address in file `/etc/mbox.cfg`
Then restart board.

4. Debug
Program log was located at `/var/log/user.log`

# Backup and Restore fw image (SD Card)

1. Backup
Insert the SD Card to PC then list the mounted device by command:
```
$ df -h
/dev/sde3       2.8G  4.4M  2.7G   1% /media/d-dragon/data
/dev/sde1        56M   19M   37M  34% /media/d-dragon/boot
/dev/sde2       4.2G  4.2G     0 100% /media/d-dragon/13d368bf-6dbf-4751-8ba1-88bed06bef77
```

sde is the SD Card device, it could be different (sdb, sdc, etc.) device name on different PC.
Enter the following command to backup SD Card data into an image:
```
$ sudo dd if=/dev/sde of=./mbox-fw-image-v1.img
```

2. Restore
Use the backup image in above step to restore the firmware. First, we need to umount the partition.
```
$ sudo umount /dev/sde1
$ sudo umount /dev/sde2
$ sudo umount /dev/sde3
```

Then write the image to SD Card by command:
```
$ sudo dd bs=4M if=~/mbox-fw-image-v1.img of=/dev/sde
```

Hope everything would be good :).
Owner: Duy Phan
