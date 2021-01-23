
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../include
LFLAGS = -L../..
LIBS = -lcn

OBJDIR = .ofiles
SRCS =	main.c input.c parser.c operator.c

#CFLAGS = -DPREVIOUS
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = system

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)
	cp ../../libcn.so .

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN)
	$(RM) *.so

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
