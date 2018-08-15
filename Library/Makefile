
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include

OBJDIR = .ofiles
SRCS =	pair.c list.c string_util.c system.c

#CFLAGS = -DPREVIOUS
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET = libcn.so

.PHONY: depend clean

all:$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -shared -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(TARGET)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
