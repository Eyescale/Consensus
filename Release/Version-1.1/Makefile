
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	database.c \
	db_io.c \
	db_op.c \
	db_debug.c \
	context.c \
	expression.c \
	narrative.c \
	operation.c \
	traversal.c

LIB = libbmod.a

TARGETS = $(LIB) B%

LFLAGS = -L. -L../../Base
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../Base/include
#CFLAGS = -DDEBUG -DNULL_TERMINATED
#CFLAGS = -O2 -DUNIFIED
CFLAGS = -O2 -DOPT

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

