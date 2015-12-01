PROJECT	= Mbox
CC		= gcc

CFLAGS	= -Wall -g -O0 -D RPI -D DEBUG -I./inc -I/usr/include/python2.7 -I/usr/include/libxml2 -I/usr/include/curl
DEPFLAGS= -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE= mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
LIBS	= -pthread -lrt -lpython2.7 -lxml2 -lconfig -lwiringPi -lm -lcurl

OBJDIR	= obj
BINDIR 	= bin
DEPDIR	= dep
FILEDIR	= files
SOURCEDIR = src

SRCFILES := $(wildcard $(SOURCEDIR)/*.c)
OBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(OBJDIR)/%.o,$(SRCFILES))

all: directories Mbox

directories:
	@clear
	@echo "********Create directories*******"
	@mkdir -p $(OBJDIR)
	@mkdir -p $(BINDIR)
	@mkdir -p $(DEPDIR)
	@mkdir -p $(FILEDIR)


Mbox: $(OBJECTS)
	@echo "******Build Mbox application******"
	$(CC) -o $(BINDIR)/$@ $^ $(CFLAGS) $(LIBS)

$(OBJDIR)/%.o: $(SOURCEDIR)/%.c $(DEPDIR)/%.d
	@echo "**********Build objects**********"
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@
	$(POSTCOMPILE)

$(DEPDIR)/%.d: ;

.PHONY: clean install
 
install:
	cp bin/$(PROJECT) /usr/bin/
	cp files/startMediaHub.sh /usr/bin/
	cp files/omxplayer_dbus_control.sh /usr/bin/
	cp src/ftplib_example.py /usr/bin/

clean:
	rm -rf $(BINDIR) $(DEPDIR) $(OBJDIR)

#include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCFILES)))
