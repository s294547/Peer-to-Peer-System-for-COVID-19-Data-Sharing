#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "peer_struct.h"
//variabile globale contente il numero di porta del peer
extern int peer_id;
//variabile globale contenente l'indirizzo ip del peer
extern char* peer_ip;
//numero di porta e indirizzo ip del server a cui il peer si connetterà
extern int server_port;
extern char* server_ip;
//socket tcp usato per la comunicazione con il server
extern int server_sock;
//descrittore e indirizzo del socket attualmente utilizzato dal peer
extern int peer_sock;
extern struct sockaddr_in peer_addr;
//numeri di porta dei vicini, se corrispondo a -1 non c'è il vicino
extern int neigh[];
//la funzione start può essere chiamata (con successo) una sola volta dal boot
extern int start_called;
//lista dei daily registers
extern struct daily_register* registers;
//lista dei dai aggregati disponibili sul peer
extern struct data_aggr* aggr;
extern int fdmax; // Numero max di descrittori
extern fd_set master; /* Set principale, deve essere condiviso dato che in certe funzioni è possibile definire dei set più piccoli*/
extern const char* comandi[];
//lunghezza dei codici 
#define CODE_LEN 6

int count_aggr();
int count_registers();
struct data_aggr* create_aggr(int, int, int ,int , int , int , char , char , int );
struct daily_register* create_daily_register(int , int, int);
struct daily_register* create_register(int, int, int, int, int, int );
struct data_aggr* find_aggr(int, int, int, int, int , int , char , char );
struct daily_register* find_daily_register(int, int, int);
int get_next_day(int, int, int);
int get_next_month(int, int);
int get_next_year(int, int, int);
int get_prev_day(int, int, int);
int get_prev_month(int, int);
int get_prev_year(int, int, int);