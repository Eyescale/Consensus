
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	pair.c \
	list.c \
	entity.c \
	registry.c \
	string_util.c \
	btree.c

LIB = libcn.a

TARGETS = $(LIB) btreefy

LFLAGS = -L.
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include
CFLAGS = -O2

all:$(TARGETS)

btreefy: $(LIB)
	$(CC) -o btreefy -DTEST $(INCLUDES) btree.c $(LFLAGS) -lcn

$(LIB): $(OBJS)
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(LIB) btreefy

depend: $(SRCS)
	makedepend $(INCLUDES) $^

