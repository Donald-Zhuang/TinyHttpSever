CC	= gcc
OBJECT 	= 
CFLAGS  = -g -Wall -O3
LDFLAGS =
LDLIBS	= -lpthread
all:sever client

sever:sever.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS)
client:client.c
	$(CC) $^ $(CFLAGS) -o $@ $(LDLIBS)
clean:
	rm -rf ./*.o ./*.out 
