# Makefile for A Walk in a Park
#
#  (c) 2008-2011  Scott & Linda Wills

# The compiler being used

CC= gcc

# compiler options

CFLAGS= -g -lm -Wall -MMD -MP
# CFLAGS= -O2 -lm

# linker

LN= $(CC)

# linker options

LNFLAGS= -g -lm
# LNFLAGS= -lm

# extra libraries used in linking (use -l command)

LDLIBS= /usr/lib/libjpeg.so.62

# source files

SOURCES= P3-1.c mmm.c utils.c rollers.c

# include files

INCLUDES= mmm.h utils.h rollers.h

# object files

OBJECTS= P3-1.o mmm.o utils.o rollers.o

all: P3-1

.c.o:	$*.c
	$(CC) $(CFLAGS) -c $*.c

P3-1:	$(OBJECTS)
	$(LN) $(LNFLAGS) -o $@ $(OBJECTS) $(LDLIBS)

clean:
	rm -f *.o
	rm -f *.d
	rm -f *~

test:	P3-1
	./P3-1 "InSeq" 1 180 1

videos:	
	mencoder mf://seqs/InSeq/*.jpg -mf w=640:h=140:fps=4:type=jpg \
	-ovc lavc -lavcopts vcodec=mpeg4:mbd=2:trell -oac copy -o seqs/InSeq.mpg
	mencoder mf://trials/InSeq/rs*.jpg -mf w=640:h=560:fps=4:type=jpg \
	-ovc lavc -lavcopts vcodec=mpeg4:mbd=2:trell -oac copy -o trials/InSeq.mpg
	mencoder mf://trials/InSeq/out*.jpg -mf w=640:h=560:fps=4:type=jpg \
	-ovc lavc -lavcopts vcodec=mpeg4:mbd=2:trell -oac copy -o trials/InSeq.mpg

-include $(SOURCES:%.c=%.d)
