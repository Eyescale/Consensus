
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS = smgen.c
TARGETS = smgen

LFLAGS = -L. -L../..
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../include
CFLAGS = -DTEST -O2

all:$(TARGETS)

smgen: $(OBJDIR) $(OBJS) ../../libcn.a
	$(CC) -o smgen $(INCLUDES) $(OBJS) $(LFLAGS) -lcn

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles
