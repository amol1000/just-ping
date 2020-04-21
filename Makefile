all: ping.c
	gcc -g -Wall -o ping ping.c

clean:
	$(RM) ping
