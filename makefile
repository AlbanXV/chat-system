CFLAGS = -std=gnu11 -g -Wall -Wextra
VFLAGS = --leak-check=full --show-leak-kinds=all --track-origins=yes --malloc-fill=0x40 --free-fill=0x23
BIN = server client

all: $(BIN)

server: server.o send_packet.o send_packet.h
	gcc $(CFLAGS) $^ -o server

client: client.o send_packet.o send_packet.h
	gcc $(CFLAGS) $^ -o client

server.o: server.c
	gcc $(CFLAGS) $^ -c server.c

client.o: client.c
	gcc $(CFLAGS) $^ -c client.c

send_packet.o: send_packet.c
	gcc $(CFLAGS) $^ -c send_packet.c

checkS: server
	valgrind $(VFLAGS) ./server

checkC: client
	valgrind $(VFLAGS) ./client
	
clean:
	rm -f $(BIN) *.o
