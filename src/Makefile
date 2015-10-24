#CC=arm-linux-gnueabihf-gcc
APP=Mbox
CC=gcc
CFLAGS=-I. -Wall
#PLAYAUDIOFLAG=-D AUDIO_ENABLE
DEBUGFLAG=-D DEBUG
TARGET = pi
DEPS = FileHandler.h sock_infra.h advertisement.h logger.h playAudio.h acpHandler.h xmlHandler.h
OBJS = audio_app.o FileHandler.o advertisement.o sock_infra.o logger.o playAudio.o acpHandler.o xmlHandler.o

		
.c.o: $(DEPS)
ifneq ($(TARGET),pi)
	$(CC) $(CFLAGS) $(DEBUGFLAG) -I/usr/include/python2.7 -I/usr/include/libxml2 -g -O0 -c -o $@  $<
else
	$(CC) $(CFLAGS) $(DEBUGFLAG) -D RPI -I/usr/include/python2.7 -I/usr/include/libxml2 -g -O0 -c -o $@  $<
endif
			
audio_app: $(OBJS)
ifneq ($(TARGET),pi)
	$(CC) -o $(APP) $^ $(CFLAGS) $(DEBUGFLAG) -pthread -lpython2.7 -lxml2 -lconfig
else
	$(CC) -o $(APP) $^ $(CFLAGS) -D RPI -pthread -lrt -lasound -ljack -lpython2.7 -lxml2 -lconfig -lmpg123 -lm
endif
			
	
install:
#ifeq ($(TARGET),pi)
#ifneq ("$(wildcard $(/usr/bin/MediaHub))","")
#	rm /usr/bin/MediaHub
#endif
#ifeq ("$(wildcard $(/usr/bin/startMediaHub.sh))","")
#	rm /usr/bin/startMediaHub.sh
#endif
	cp $(APP) /usr/bin/
	cp startMediaHub.sh /usr/bin/
	cp omxplayer_dbus_control.sh /usr/bin/
	cp ftplib_example.py /usr/bin/
#endif

uninstall:
	rm /usr/bin/startMediaHub.sh
	rm /usr/bin/$(APP)		
.PHONY: clean

clean:
	rm *.o $(APP)



