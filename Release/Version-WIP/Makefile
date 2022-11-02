
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	narrative.c \
		parser.c \
	program.c \
		context.c \
			database.c \
			db_op.c \
			locate_param.c \
		operation.c \
			deternarize.c \
			expression.c \
				query.c \
				locate.c \
				scour.c \
			instantiate.c \
			proxy.c
LIB = libbmod.a

TARGETS = $(LIB) B%

LFLAGS = -L. -L../../Base
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../Base/include
#CFLAGS = -O2
#CFLAGS = -DDEBUG -DNULL_TERMINATED
CFLAGS = -O2

all:$(TARGETS)

B%: $(LIB) main.c ../../Base/libcn.a
	$(CC) -o B% $(INCLUDES) main.c $(LFLAGS) -lcn -lbmod

$(LIB): $(OBJDIR) $(OBJS) 
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles
