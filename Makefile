
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I.

OBJDIR = .ofiles
SRCS =	api.c expression.c frame.c kernel.c narrative_util.c string_util.c command.c \
	expression_solve.c hcn.c main.c output.c value.c database.c expression_util.c \
	input.c narrative.c registry.c variables.c filter_util.c

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
