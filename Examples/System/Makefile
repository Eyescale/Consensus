
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../Library/include
LFLAGS = -L../../Library
LIBS = -lcn

OBJDIR = .ofiles
SRCS =	main.c Input.c Parser.c

#CFLAGS = -DPREVIOUS
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = System

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN)
	cp ../../Library/libcn.so .

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
