
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../include
LFLAGS = -L../..
LIBS = -lcn

OBJDIR = .ofiles
SRCS =	main.c scheme.c \
	parser.c parser_debug.c \
	regex.c regex_debug.c

CFLAGS = -DDEBUG
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = yak

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)
	cc -shared -o libyak.so $(INCLUDES) parser.c scheme.c regex.c $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN) libyak.so
	cp ../../libcn.so .

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
