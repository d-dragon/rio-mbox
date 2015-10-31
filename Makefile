PROJECT	= Mbox
CC		= gcc

CFLAGS	= -Wall -g -O0 -I./inc -I/usr/include/python2.7 -I/usr/include/libxml2 
DEPFLAGS= -MT $@ -MMD -MP -MF $(DEPDIR)/$*.Td
POSTCOMPILE= mv -f $(DEPDIR)/$*.Td $(DEPDIR)/$*.d
LIBS	= -pthread -lrt -lpython2.7 -lxml2 -lconfig -lm

OBJDIR	= obj
BINDIR 	= bin
DEPDIR	= dep
FILEDIR	= files
SOURCEDIR = src


#SRCFILES := $(shell find . -name '*.c')
#OBJECTS 	:= $(patsubst %.c,$(OBJDIR)/%.o,$(SRCFILES))
SRCFILES := $(wildcard $(SOURCEDIR)/*.c)
OBJECTS := $(patsubst $(SOURCEDIR)/%.c,$(OBJDIR)/%.o,$(SRCFILES))

all: directories Mbox

directories:
	@clear
	@echo "********Create directories*******"
	@echo $(DIRS) 
	@echo $(SUBDIRS)
#	@echo $(SRCFILES)/*.c
	@echo $(SRCFILES)
	@echo $(OBJECTS)
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

include $(patsubst %,$(DEPDIR)/%.d,$(basename $(SRCFILES)))
