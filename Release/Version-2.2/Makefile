
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	story.c \
		narrative.c \
		parser.c \
		parser_io.c \
	program.c \
		cell.c \
			context.c \
				database.c \
				db_arena.c \
				db_op.c \
		operation.c \
			deternarize.c \
			instantiate.c \
				eenov.c \
					locate_emark.c \
			expression.c \
				query.c \
					locate_pivot.c \
					locate_mark.c \
			eeno_query.c \
	errout.c

LIB = libbmod.a

TARGETS = $(LIB) B%

LFLAGS = -L. -L../../Base
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../Base/include
#CFLAGS = -DDEBUG
CFLAGS = -O2

all:$(TARGETS)

B%: $(LIB) main.c ../../Base/libcn.a
	$(CC) -o B% $(INCLUDES) main.c $(LFLAGS) -lcn -lbmod

$(LIB): $(OBJDIR) $(OBJS) 
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o:%.c include/traversal.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles
