
SHELL = /bin/sh

.SUFFIXES: .c .o

SRCS = main.c translate.c

TARGETS = convert

LFLAGS = -L../1_Yak/ -L../..
OBJDIR = .ofiles
OBJS = $(SRCS:%.c=$(OBJDIR)/%.o)
INCLUDES = -I. -I./include -I../../include -I../1_Yak/include
CFLAGS = -O2

all:$(TARGETS)

convert: $(OBJDIR) $(OBJS) ../../libcn.a ../1_Yak/libyak.a
	$(CC) -o convert $(OBJS) $(LFLAGS) -lcn -lyak

$(OBJS): $(OBJDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $<  -o $@

$(OBJDIR):
	mkdir .ofiles

clean:
	$(RM) $(OBJDIR)/*.o $(TARGETS)
	rmdir .ofiles

