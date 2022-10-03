
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS = segmentize.c
TARGETS = segmentize

LFLAGS = -L. -L../..
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../include
CFLAGS = -O2

all:$(TARGETS)

segmentize: $(OBJDIR) $(OBJS) ../../libcn.a
	$(CC) -o segmentize $(INCLUDES) $(OBJS) $(LFLAGS) -lcn

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles
