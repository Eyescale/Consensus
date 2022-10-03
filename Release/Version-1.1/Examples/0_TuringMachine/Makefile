
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../../Base/include -I../../Version-1.0/include
LFLAGS = -L../../../Base -L../../Version-1.0
LIBS = -lcn -lbmod

OBJDIR = .ofiles
SRCS = main.c

#CFLAGS = -DDEBUG
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = B%

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)
	cp ../../Version-1.0/libbmod.so .
	cp ../../../Base/libcn.so .

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN)
	$(RM) *.so

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
