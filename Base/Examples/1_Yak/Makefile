
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS =	main.c scheme.c \
	parser.c parser_debug.c \
	regex.c regex_debug.c

LIB = libyak.a
TARGETS = yak $(LIB)

LFLAGS = -L. -L../..
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../include
#CFLAGS = -DDEBUG
CFLAGS = -O2

all:$(TARGETS)

yak: $(OBJDIR) $(LIB) main.c ../../libcn.a
	$(CC) -o yak $(CFLAGS) $(INCLUDES) main.c $(LFLAGS) -lcn -lyak

$(LIB): $(OBJS)
	ar -rcvus $(LIB) $(OBJS)

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles

