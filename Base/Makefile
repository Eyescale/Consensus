
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	registry.c \
		pair.c \
		list.c \
	btree.c \
	string_util.c \
		prune.c \
	entity.c

LIB = libcn.a

TARGETS = $(LIB)

LFLAGS = -L.
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include
CFLAGS = -O2

all:$(TARGETS)

$(LIB): $(OBJDIR) $(OBJS)
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(LIB) btreefy
	rmdir .ofiles

