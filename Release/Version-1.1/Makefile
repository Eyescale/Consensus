
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include -I../../Base/include
LFLAGS = -L../../Base
LIBS = -lcn

OBJDIR = .ofiles
SRCS =	database.c \
	db_io.c \
	db_op.c \
	context.c \
	expression.c \
	narrative.c \
	operation.c \
	traversal.c \
	wildcard.c

#CFLAGS = -O2
CFLAGS = -O2 -DUNIFIED
#CFLAGS = -DDEBUG -DTRIM
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
TARGET = libbmod.so

.PHONY: depend clean

all:$(TARGET) B%

B%: $(OBJS)
	$(CC) -o B% $(INCLUDES) main.c -L. -lcn -lbmod

$(TARGET): $(OBJS) 
	$(CC) -shared -o $(TARGET) $(OBJS) $(LFLAGS) $(LIBS)
	cp ../../Base/libcn.so .

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN)
	$(RM) *.so

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
