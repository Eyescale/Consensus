
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	narrative.c \
		parser.c \
	program.c \
		database.c \
		context.c \
		operation.c \
			db_op.c \
			deternarize.c \
			expression.c \
			instantiate.c \
			query.c \
				locate.c \
				traverse.c
LIB = libbmod.a

TARGETS = $(LIB) B%

LFLAGS = -L. -L../../Base
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../Base/include
CFLAGS = -O2
#CFLAGS = -DDEBUG -DNULL_TERMINATED
#CFLAGS = -O2 -DUNIFIED

all:$(TARGETS)

B%: $(LIB) main.c ../../Base/libcn.a
	$(CC) -o B% $(INCLUDES) main.c $(LFLAGS) -lcn -lbmod

$(LIB): $(OBJS) 
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)

