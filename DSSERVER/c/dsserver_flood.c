#include "../h/dsserver_data.h"

//funzione per la gestione dei flooding, durante un qualasi flooding non possono arrivare richieste di START o SSTOP
void gestione_flood(int sock_flood){
	fd_set local_master; /* Set locale gestito dal programmatore con le macro */
	fd_set read_fds; /* Set di lettura gestito dalla select */
	int local_fdmax; // Numero max di descrittori
	int ret=0, send_ret=0, tot_flood=0, i;
	char buff[1024];
	FD_ZERO(&local_master);
	FD_ZERO(&read_fds);
	//il set locale sarà identico al set master, però non contiene stdin e il socket udp (non si accettano comandi dal terminale o start/stop dai peer)
	local_master=master;
	FD_CLR(0, &local_master);
	FD_CLR(server, &local_master);
	// Tengo traccia del maggiore 
	local_fdmax = fdmax;
	//i flood 'attivi' sono due, uno dai vicni a sinistra e uno dai vicini a destra
	tot_flood=2;
	//invio del messaggio 'FLACK' al peer per abilitarlo al flooding
	do{	
		strncpy(buff, "FLACK", 5);
		buff[5]='\0';
		ret=send(sock_flood, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	for(;;){
		read_fds = local_master; // read_fds sarà modificato dalla select
		select(local_fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=local_fdmax; i>=0; i--) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto
				//ricezione del messaggio
				ret = recv(i, buff, CODE_LEN, 0);
				// se arriva una nuova richiesta di flooding
				if(ret==CODE_LEN && strcmp(buff,"FLNEW")==0){
					//aggiorno il conteggio dei flooding attivi
					tot_flood+=2;
					//invio del messaggio 'FLACK' al peer per abilitarlo al flooding
					do{	
						strncpy(buff, "FLACK", 5);
						buff[5]='\0';
						send_ret=send(i, buff, CODE_LEN, 0);
					}while(send_ret<CODE_LEN);
				}
				//se il server riceve una richiesta di terminazione viene rifiutata poichè il server è impegnato in un flooding
				if(ret==CODE_LEN && strcmp(buff,"SSTOP")==0){
					//invio "SNACK" per rifiutare la richiesta di terminazione
					strncpy(buff,"SNACK",5);
					buff[5]='\0';
					do{
						send_ret=send(i, buff, CODE_LEN, 0);
					}while(send_ret<CODE_LEN);
				}
				// se da un lato è terminato il flooding
				if(ret==CODE_LEN && strcmp(buff,"FLEND")==0){
					//decremento la variabile contenente il numero di flood attivi
					tot_flood--;
					//se non ci sono più flood in corso, posso tornare ad accettare comandi da stdin
					if(tot_flood==0)
						return;
				}
			}
		} // Fine for f1
	}
}