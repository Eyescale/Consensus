
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../Base/include -I../../Yak/include
LFLAGS = -L../../Base -L../../Yak
LIBS = -lcn -lyak

OBJDIR = .ofiles
SRCS =	main.c translate.c

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = translate

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o ../base/.ofiles/*.o $(MAIN)
	cp ../../Base/libcn.so .
	cp ../../Yak/libyak.so .

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
