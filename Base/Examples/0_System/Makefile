
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS = main.c input.c parser.c operator.c

TARGETS = system

LFLAGS = -L. -L../..
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../include
CFLAGS = -O2

all:$(TARGETS)

system: $(OBJDIR) $(OBJS) ../../libcn.a
	$(CC) -o system $(INCLUDES) $(OBJS) $(LFLAGS) -lcn

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles

