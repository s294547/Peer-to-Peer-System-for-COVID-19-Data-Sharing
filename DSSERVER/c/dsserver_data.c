#include "../h/dsserver_struct.h"
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
fd_set master; //set principale gestito dal programmatore con le macro 
int fdmax; // numero max di descrittori
//lunghezza dei codici dei messaggi
#define CODE_LEN 6
//numero di porta del server
int server_port;
//descrittore di socket del server
int server;
//dichiarazione e inizializzazione della lista dei peer
struct Peer* lista_peer=NULL;
//mantengo memorizzati i possibili comandi
const char* comandi[4]={"help", "showpeer", "showneighbor", "esc"};