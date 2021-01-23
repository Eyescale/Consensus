
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../include -I../1_Yak/include
LFLAGS = -L../.. -L../1_Yak
LIBS = -lcn -lyak

OBJDIR = .ofiles
SRCS =	main.c translate.c

OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = convert

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)
	cp ../../libcn.so .
	cp ../1_Yak/libyak.so .

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o ../base/.ofiles/*.o $(MAIN)
	$(RM) *.so

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
