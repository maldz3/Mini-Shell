# Program name: Program 3 - smallsh
# Author: Maliha Syed
# Date: 2/13/20
# Description: Makefile for Program 3


C = gcc
CFLAGS = -std=gnu99
CFLAGS += -Wall
OBJS = smallsh.o
SRCS = smallsh.c

smallsh: $(OBJS)
	$(C) $(CFLAGS) $(OBJS) -o smallsh

smallsh.o: smallsh.c
	$(C) $(CFLAGS) -c smallsh.c

.PHONY : clean
clean :
	-rm *.o smallsh

zip:
	zip syedm_program3.zip smallsh.c makefile README -D

