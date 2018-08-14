
# from http://www.cs.swarthmore.edu/~newhall/unixhelp/howto_makefiles.html

SHELL = /bin/sh

.SUFFIXES:
.SUFFIXES: .c .o

INCLUDES = -I. -I./include

OBJDIR = .ofiles
SRCS =	main.c command.c command_util.c \
	narrative.c narrative_util.c \
	expression.c expression_util.c expression_solve.c expression_sieve.c \
	path.c value.c variable.c variable_util.c \
	io.c input.c input_util.c hcn.c output.c output_util.c \
	frame.c api.c kernel.c database.c list.c registry.c string_util.c xl.c \
	cgic.c

#CFLAGS = -DPREVIOUS
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
