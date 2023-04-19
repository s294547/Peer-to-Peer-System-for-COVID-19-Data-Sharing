# make rule primaria 
all: peer ds

# make rule per il peer
peer : peerfinal.o peer_get.o peer_add.o peer_start.o peer_stop.o peerutilfunc.o peerdata.o
	gcc -o peer peerfinal.o peer_get.o peer_add.o peer_start.o peer_stop.o peerutilfunc.o peerdata.o
peerfinal.o : PEER/c/peerfinal.c PEER/h/peer_add.h PEER/h/peer_start.h PEER/h/peer_get.h PEER/h/peer_stop.h PEER/h/peerutilfunc.h PEER/h/peerdata.h 
	gcc -Wall -c PEER/c/peerfinal.c
peer_get.o : PEER/c/peer_get.c PEER/h/peer_stop.h PEER/h/peerutilfunc.h PEER/h/peerdata.h 
	gcc -Wall -c PEER/c/peer_get.c
peer_add.o : PEER/c/peer_add.c PEER/h/peer_stop.h PEER/h/peerutilfunc.h PEER/h/peerdata.h 
	gcc -Wall -c PEER/c/peer_add.c
peer_start.o : PEER/c/peer_start.c PEER/h/peerutilfunc.h PEER/h/peerdata.h 
	gcc -Wall -c PEER/c/peer_start.c
peer_stop.o : PEER/c/peer_stop.c PEER/h/peerutilfunc.h PEER/h/peerdata.h 
	gcc -Wall -c PEER/c/peer_stop.c
peerutilfunc.o : PEER/c/peerutilfunc.c PEER/h/peerdata.h
	gcc -Wall -c PEER/c/peerutilfunc.c
peerdata.o :PEER/c/peerdata.c PEER/h/peer_struct.h
	gcc -Wall -c PEER/c/peerdata.c


# make rule per il dsserver
ds : dsserver_final.o dsserver_data.o dsserver_start.o dsserver_flood.o dsserver_stop.o dsserver_comandi.o 
	gcc -o  ds dsserver_final.o dsserver_data.o dsserver_start.o dsserver_flood.o dsserver_stop.o dsserver_comandi.o 
dsserver_final.o : DSSERVER/c/dsserver_final.c DSSERVER/h/dsserver_data.h DSSERVER/h/dsserver_start.h DSSERVER/h/dsserver_flood.h DSSERVER/h/dsserver_stop.h DSSERVER/h/dsserver_comandi.h 
	gcc -Wall -c DSSERVER/c/dsserver_final.c
dsserver_comandi.o : DSSERVER/c/dsserver_comandi.c DSSERVER/h/dsserver_data.h DSSERVER/h/dsserver_stop.h
	gcc -Wall -c DSSERVER/c/dsserver_comandi.c
dsserver_stop.o : DSSERVER/c/dsserver_stop.c DSSERVER/h/dsserver_data.h DSSERVER/h/dsserver_start.h DSSERVER/h/dsserver_flood.h
	gcc -Wall -c DSSERVER/c/dsserver_stop.c
dsserver_start.o : DSSERVER/c/dsserver_start.c DSSERVER/h/dsserver_data.h
	gcc -Wall -c DSSERVER/c/dsserver_start.c
dsserver_flood.o : DSSERVER/c/dsserver_flood.c DSSERVER/h/dsserver_data.h
	gcc -Wall -c DSSERVER/c/dsserver_flood.c
dsserver_data.o : DSSERVER/c/dsserver_data.c DSSERVER/h/dsserver_struct.h
	gcc -Wall -c DSSERVER/c/dsserver_data.c

# pulizia dei file della compilazione
clean:
	rm *.o peer ds