
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include

OBJDIR = .ofiles
SRCS =	pair.c \
	list.c \
	registry.c \
	string_util.c \
	btree.c

CFLAGS = -O2
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET = libcn.so

.PHONY: depend clean

all:$(TARGET) btreefy

btreefy: $(OBJS)
	$(CC) -o btreefy -DBT_TEST -I./include btree.c -L. -lcn

$(TARGET): $(OBJS)
	$(CC) -shared -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(TARGET)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
