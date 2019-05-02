CC = gcc
CFLAGS = -Wall -g
RM = rm -f

default: all

all: oss user

oss: oss.c
	$(CC) $(CFLAGS) -o oss oss.c
	
user: user.c
	$(CC) $(CFLAGS) -o user user.c

clean veryclean:
	$(RM) oss user
