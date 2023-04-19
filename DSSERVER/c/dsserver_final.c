#include "../h/dsserver_data.h"
#include "../h/dsserver_start.h"
#include "../h/dsserver_stop.h"
#include "../h/dsserver_comandi.h"
#include "../h/dsserver_flood.h"

int main(int argc, char* argv[]){
	int i=0, ret=0, send_ret;
	fd_set read_fds; /* Set di lettura gestito dalla select */
	struct sockaddr_in sv_addr; // Indirizzo server
	struct sockaddr_in cl_addr; // indirizzo peer
	int addrlen = sizeof(cl_addr);
	char buff[1024]; // buffer
	//controllo che sia stato inserito un numero di porta
	ret=controllo_argomenti(argc, argv);
	if(ret==-1){
		printf("Numero di porta non valido\n");
		return 0;
	}
	//salvo il numero di porta del server sulla variabile globale
	server_port=ret;
	printf("***************************** DS COVID RUNNING *****************************\n");
	printf("Digita un comando:\n");
	printf("\n");
	printf("1) help --> mostra i dettagli dei comandi\n");
	printf("2) showpeer --> mostra un elenco dei peer connessi\n");
	printf("3) showneighbor <peer> --> mostra i neighbor di un peer\n");
	printf("4) esc --> chiude il DS\n\n");
	printf("_\n");

	/* Azzero i set */
	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	server = socket(AF_INET, SOCK_DGRAM, 0);
	memset(&sv_addr, 0, sizeof(sv_addr));
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1" , &sv_addr.sin_addr);
	sv_addr.sin_port = htons(server_port);
	ret=bind(server, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
	if(ret<0){
		if (errno == EADDRINUSE)
				printf("Non è stato possibile effettuare la bind perchè la porta scelta è già in utilizzo. Scegliere un numero di porta differente.\n");
		if(errno == EACCES)
				printf("Non è stato possibile effettuare la bind perchè non si hanno sufficienti privilegi per la porta scelta . Scegliere un numero di porta differente.\n");
		return 0;
	}
	// Aggiungo il server al set
	FD_SET(server, &master);
	//aggiungo stdin al set
	FD_SET(0, &master);
	// Tengo traccia del maggiore (ora è il listener)
	fdmax = server;
	for(;;){
		FD_SET(0, &master);
		read_fds = master; // read_fds sarà modificato dalla select
		select(fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=fdmax; i>=0; i--) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto

				if(i == server) { // se è arrivato un messaggio sul socket udp (start/stop di un peer)
					ret=recvfrom(server, buff, 6, 0, (struct sockaddr*)&cl_addr, (socklen_t*)&addrlen);
					buff[5]='\0';
					if(ret==6 && !strcmp(buff, "START")){
						//creo il peer
						set_peer(ntohs(cl_addr.sin_port));
						//invio dell'ack
						strncpy(buff, "ACK", 4);
						sendto(server, buff, 4, 0, (struct sockaddr*)&cl_addr, (socklen_t)addrlen);
						//aggiornamento dei vicini sulla struttura dati e notifica dei peer
						update_and_send_neighs(ntohs(cl_addr.sin_port));
					}
					
				}
				//se è arrivato un messaggio da un peer
				if(i!=0 && i!=server){
					ret=recv(i, buff, CODE_LEN, 0);
					//se il messaggio ricevuto è una richiesta di flooding, sospendo le attività per dedicarmi solo ai flooding
					if(ret==CODE_LEN && strcmp(buff, "FLNEW")==0)
						gestione_flood(i);
					//se il server riceve una richiesta di terminazione viene accettata, non è impegnato in flooding
					if(ret==CODE_LEN && strcmp(buff,"SSTOP")==0){
						//invio "STACK" per accettare la richiesta di terminazione
						strncpy(buff,"STACK",5);
						buff[5]='\0';
						do{
							send_ret=send(i, buff,CODE_LEN, 0);
						}while(send_ret<CODE_LEN);
						stop_peer(i);
					}
				}			
				// è stato inserito qualcosa da terminale
				if(i==0){				
					esegui_comando();
				}
			}
		} // Fine for f1
	} // Fine for(;;)
}
