
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I.

OBJDIR = .ofiles
SRCS =	main.c command.c command_util.c \
	narrative.c narrative_util.c \
	expression.c expression_util.c expression_solve.c expression_filter.c \
	path.c value.c variables.c \
	io.c input.c input_util.c hcn.c output.c \
	frame.c api.c kernel.c database.c registry.c

CFLAGS =
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
MAIN = consensus

.PHONY: depend clean

all:$(MAIN)

$(MAIN): $(OBJS) 
	$(CC) $(CFLAGS) $(INCLUDES) -o $(MAIN) $(OBJS) $(LFLAGS) $(LIBS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

clean:
	$(RM) $(OBJDIR)/*.o $(MAIN)

depend: $(SRCS)
	makedepend $(INCLUDES) $^

# DO NOT DELETE THIS LINE -- make depend needs it
