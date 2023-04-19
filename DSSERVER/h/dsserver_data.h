#include "dsserver_struct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
extern fd_set master; //set principale gestito dal programmatore con le macro 
extern int fdmax; // numero max di descrittori
//lunghezza del codice dei messaggi
#define CODE_LEN 6
//numero di porta del server
extern int server_port;
//descrittore di socket del server
extern int server;
//dichiarazione e inizializzazione della lista dei peer
extern struct Peer* lista_peer;
//mantengo memorizzati i possibili comandi
extern const char* comandi[];