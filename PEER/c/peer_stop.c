#include "../h/peerdata.h"
#include "../h/peerutilfunc.h"

//funzione per il salvataggio dei registri giornalieri su file e per la loro deallocazione
void write_and_delete_register(FILE* file_saved){
	char app=' ';
	struct daily_register* appoggio=registers;
	registers=registers->next_register;
	//scrittura del registro che si trova in testa su file (salvo i registri in ordine cronologico)
	fprintf(file_saved,"%d%c%d%c%d%c%d%c%d%c%d%c", appoggio->day, app, appoggio->month, app, appoggio->year, app, appoggio->tot_tamponi, app, appoggio->tot_casi, app, appoggio->closed, app);
	//deallocazione del registro
	free(appoggio);
}

//funzione per il salvataggio dei dati aggregati su file e per la loro deallocazione
void write_and_delete_aggr(FILE* file_saved){
	char app=' ';
	struct data_aggr* appoggio=aggr;
	int ret=0;
	aggr=aggr->next_aggr;

	//scrittura del dato aggregto che si trova in testa su file 
	fprintf(file_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", appoggio->day1, app, appoggio->month1, app, appoggio->year1, app, appoggio->day2, app, appoggio->month2, app, appoggio->year2, appoggio->aggrt, ret, appoggio->type, appoggio->data, app);
	//deallocazione del dato aggregato
	free(appoggio);
}


//funzione per l'invio (in fase di disconnessione dal dsserver) dei propri registri e dati aggregati a un vicino o al server se sono l'unico peer ad esso connesso 
void send_registers_and_aggr_to_neigh(int tot_registers, int tot_aggr, int i){
	int vicino=-1, comm_sock, ret, num_reg;
	char buff[1024];
	char app=' ';
	uint16_t msglen;
	struct daily_register* appoggio=registers;
	struct data_aggr* appoggio2=aggr;
	struct sockaddr_in neigh_addr;
	int addrlen=sizeof(neigh_addr);
	//se dobbiamo mandare i registri a un vicino
	if(i!=2){
		//se il vicino non è presente, termina
		if(neigh[i]==-1)
			return;
		else
			vicino=neigh[i];
		//creazione dell'indirizzo del socket tcp di uno dei vicini
		memset(&neigh_addr, 0, sizeof(neigh_addr));
		neigh_addr.sin_family = AF_INET;
		inet_pton(AF_INET, "127.0.0.1" , &neigh_addr.sin_addr);
		neigh_addr.sin_port = htons(vicino);
		//creazione del socket tcp per la comunicazione con il vicino
		printf("Creazione del socket TCP per l'invio dei registri e dei dati aggregati del peer %d al vicino con porta:%d\n", peer_id, neigh[i]);
		comm_sock=socket(AF_INET, SOCK_STREAM, 0);
		//connessione al vicino
		ret=connect(comm_sock,(struct sockaddr*)&neigh_addr, addrlen);
		printf("Connesso al socket TCP del vicino con porta:%d\n", vicino);
	}
	//se dobbiamo mandare i registri e i dati aggregati al server, è già presente il socket per la comunicazione con esso
	else{
		comm_sock=server_sock;
	}
	printf("Inizio del trasferimento dei registri giornalieri e dei dati aggregati\n");
	//invio del codice 'STREG' per avviare il trasferimento
	strncpy(buff, "STREG", 5);
	buff[5]='\0';
	do{
		ret=send(comm_sock, buff, CODE_LEN, 0);

	}while(ret<CODE_LEN);
	printf("Inviato codice 'STREG' per l'inizio del trasferimento\n");
	//trovo il numero di registri giornalieri
	num_reg=count_registers();
	//creazione del messaggio il numero di registri da ricevere
	sprintf(buff, "%d%c", num_reg, app);
	//invio la lunghezza del messaggio
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio il numero di registri da ricevere
	do{
		ret=send(comm_sock, buff,(strlen(buff)+1), 0);
	}while(ret<(strlen(buff)+1));
	printf("Invio il numero di registri giornalieri:%d\n", num_reg);
	//scorro la lista de registri giornalieri e invio ciascun registro al vicino/server
	while(appoggio!=NULL){
		//creazione del messaggio con il registro al vicino/server
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", appoggio->day, app, appoggio->month, app, appoggio->year, app, appoggio->tot_tamponi, app, appoggio->tot_casi, app, appoggio->closed, app);
		//invio la lunghezza del messaggio 
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del registro al vicino/server
		do{
			ret=send(comm_sock, buff,(strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		//prossimo registro
		appoggio=appoggio->next_register;
	}
	printf("Terminato l'invio dei registri giornalieri\n");
	//creo il messaggio con il numero di dati aggregati
	sprintf(buff, "%d%c", tot_aggr, app);
	//invio la lunghezza del messaggio 
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//inizio l'invio dei dati aggregati inviando il numero di dati aggregati
	do{
		ret=send(comm_sock, buff,(strlen(buff)+1), 0);
	}while(ret<(strlen(buff)+1));
	printf("Invio il numero di dati aggregati:%d\n", tot_aggr);
	//scorro la lista dei dati aggregati e invio ciascun dato al vicino/server
	while(appoggio2!=NULL){
		//creazione del messaggio con il dato 
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", appoggio2->day1, app, appoggio2->month1, app, appoggio2->year1, app, appoggio2->day2, app, appoggio2->month2, app, appoggio2->year2, appoggio2->aggrt, ret, appoggio2->type, appoggio2->data, app);
		//invio la lunghezza del messaggio 
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del dato al vicino/server
		do{
			ret=send(comm_sock, buff,(strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		//prossimo registro
		appoggio2=appoggio2->next_aggr;
	}
	printf("Terminato l'invio dei dati aggregati\n");
	//ricezione del codice 'SRACK' da parte del vicino/server, che apprende di aver terminato la ricezione di registri giornalieri e dei dati aggregati
	do{
		ret=recv(comm_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN || strcmp(buff, "SRACK")!=0);
	if(i!=2){
		printf("Ricevuto il codice 'SRACK' dal vicino con porta:%d\n", vicino);
		//chiusura del socket di comunicazione
		printf("Chiusura del socket TCP per la comunicazione con il vicino con porta:%d\n", vicino);
		close(comm_sock);
	}
	else
		printf("Ricevuto il codice 'SRACK' dal server\n");
}

//funzione per il salvataggio su file dei registri e dei dati aggregati e per la loro condivisione con i vicini/server
void salva_su_file(){
	int i;
	int ret;
	char app=' ';
	char buff[1024];
	FILE* file_saved;
	//totale dei registri e dei dati aggregati da salvare su file
	int tot_registers=count_registers();
	int tot_aggr=count_aggr();
	//recupero dei nome del file (peer (numero_peer))
	char filename[10];
	//invio tutti i miei registri e dati aggregati ai vicini, se ci sono
	send_registers_and_aggr_to_neigh(tot_registers,tot_aggr, 0);
	send_registers_and_aggr_to_neigh(tot_registers,tot_aggr, 1);
	//se non ho vicini, sono l'ultimo peer, quindi invio i registri e i dati aggergati al server
	if(neigh[0]==-1 && neigh[1]==-1){
		send_registers_and_aggr_to_neigh(tot_registers,tot_aggr, 2);
	}
	//informo il server che può informare i vicini del peer che sta uscendo che hanno cambiato vicini
	printf("Informo il server che può aggiornare i vicini\n");
	strncpy(buff, "STNGH", 5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	//salvataggio di registri su file
	sprintf(filename, "%s%d", "peer", peer_id);
	//apertura del file il scrittura
	file_saved=fopen(filename, "w");
	printf("Apertura del file:%s\n", filename);	
	//scrittura del numero di righe del file all'inizio del file stesso
	fprintf(file_saved, "%d%c", tot_registers, app);
	printf("Numero di registri da salvare:%d\n", tot_registers);
	//ciclo per la scrittura dei registri su file e per la deallocazione dei registri e delle entry
	for(i=0; i<tot_registers; i++){
		write_and_delete_register(file_saved);
	}
	fclose(file_saved);
	//salvataggio dei dati aggregati su file
	sprintf(filename, "%s%d", "aggr", peer_id);
	//apertura del file il scrittura
	file_saved=fopen(filename, "w");
	printf("Apertura del file:%s\n", filename);	
	//scrittura del numero di righe del file all'inizio del file stesso
	fprintf(file_saved, "%d%c", tot_aggr, app);
	printf("Numero di dati aggregati da salvare:%d\n", tot_aggr);
	//ciclo per la scrittura dei dati aggregati su file e per la deallocazione dei dati aggregatu
	for(i=0; i<tot_aggr; i++)
		write_and_delete_aggr(file_saved);
	//chiusura del file
	fclose(file_saved);
	//attende che il server notifichi che ha terminato l'invio dei vicini
	if(neigh[0]!=-1 && neigh[1]!=-1){
		do{
			ret = recv(server_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN && strcmp(buff, "NGEND")!=0);
	}
	printf("Ricevuto codice 'NGEND' dal server, vicini aggiornati\n");
	//controllo che il server abbia terminato di fare ciò che doveva fare
	do{
		ret=recv(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	if(strcmp(buff, "ENDOP")==0)
		printf("Ricevuto mesaggio 'ENDOP' dal server\n");
	//informo il server che la routine di terminazione è conclusa
	printf("Informo il server che può terminare la routine di terminazione, invio 'STEND'\n");
	strncpy(buff, "STEND", 5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	//chiusura dei vari socket aperti
	close(server_sock);
	close(peer_sock);
	return;
}

//funzione per richiedere al server di iniziare la procedura di arresto di un peer, durante l'uscita di un peer non possono essre accettate nuove richieste di connessione o di flooding
void send_stop_request(){
	fd_set local_master=master; //set locale, utilizzato per accettare nuove richieste da peer (FNEW0, FNEW1, FREQ0, FREQ1, FANS0, FANS1, GETDR, RAGGR, STREG) o dal server (STACK,SNACK, NEIGH)
	fd_set read_fds; /* Set di lettura gestito dalla select */
	char new_type;
	uint16_t msglen;
	struct sockaddr_in sock_addr;
	int local_fdmax; // Numero max di descrittori
	int ret=0, send_ret=0, i, addrlen, newfd, dir, new_quantity, day, month, year, tot_tamponi, tot_casi, num_port;
	char buff[1024], app=' ';
	//invio 'SSTOP' al server: una volta ricevuta e accettata la richiesta di terminazione dal server, il server non accetterà fino al termine dell'uscita del peer altri comandi
	strncpy(buff,"SSTOP",5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato messaggio 'SSTOP' al server per richiedere la terminazione\n");
	FD_CLR(0, &local_master);
	// Tengo traccia del maggiore 
	local_fdmax = fdmax;
	for(;;){
		read_fds = local_master; // read_fds sarà modificato dalla select
		select(local_fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=local_fdmax; i>=0; i--) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto
				if(i == peer_sock) { // se il peer riceve una richiesta di connessione 
					// se è già stata chiamata la funzione start
					if(start_called){
						addrlen = sizeof(sock_addr);
						newfd = accept(peer_sock, (struct sockaddr *)&sock_addr, (socklen_t*)&addrlen);
						//aggiunta del socket di comunicazione al set condiviso
						FD_SET(newfd, &master);
						//aggiunta del socket di comunicazione al set locale
						FD_SET(newfd, &local_master);
						if(newfd > fdmax){ fdmax = newfd; } // Aggiorno fdmax
						if(newfd > local_fdmax){ local_fdmax = newfd; } // Aggiorno local_fdmax
					}
				}		
				else{
					//ricevo il codice del messaggio
					ret = recv(i, buff, CODE_LEN, 0);
					if(i==server_sock){ // se ricevo un messaggio da parte del server

						//se il messaggio ricevuto è un STACK posso iniziare la routine di terminazione
						if(ret==CODE_LEN && strcmp(buff,"STACK")==0){
							printf("Ricevuto messaggio 'STACK' dal server, avvio la routine di terminazione\n");
							//salvataggio su file dei registri giornalieri
							salva_su_file();
							return;
						}
						//se il messaggio ricevuto è un SNACK il server è impegnato in uno o più flooding e non può accettare la richiesta di terminazione
						if(ret==CODE_LEN && strcmp(buff,"SNACK")==0){
							printf("Ricevuto messaggio 'SNACK' dal server, invio un'altra richiesta di terminazione'\n");
							//invio nuovamente la richiesta di terminazione al server
							strncpy(buff,"SSTOP",5);
							buff[5]='\0';
							do{
								send_ret=send(server_sock, buff, CODE_LEN, 0);
							}while(send_ret<CODE_LEN);
							printf("Inviato messaggio 'SSTOP' al server per richiedere la terminazione\n");
						}
						//se il messaggio ricevuto è per l'aggiunta di nuovi vicini
						if(ret==CODE_LEN && strcmp(buff,"NEIGH")==0){
							aggiorna_vicini(i);
							//se avevo mandato una richista di stop, devo rinviarla perchè probabilmente è stata persa
							strncpy(buff,"SSTOP",5);
							buff[5]='\0';
							do{
								ret=send(server_sock, buff, CODE_LEN, 0);
							}while(ret<CODE_LEN);
							printf("Inviato messaggio 'SSTOP' al server per richiedere la terminazione\n");
							continue;
						}
					}
					//se a mandare il messaggio è stato un altro peer
					if(i!=0){
						//richiesta di un dato aggregato da parte di un vicino
						if(ret==CODE_LEN && strcmp(buff, "RAGGR")==0){
								send_aggr(i);
						}
						//richiesta di un peer vicino di mandare i propri registri
						if(ret==CODE_LEN && strcmp(buff,"STREG")==0){
							printf("Ricevuto messaggio 'STREG' di un vicino, avvio la routine per ricevere i suoi registri\n");
							//salvataggio su file dei registri giornalieri
							ottieni_registri_aggr_vicino(i, 0);
						}
						//il messaggio ricevuto è una richiesta di flooding di una nuova entry del daily register
						if(ret==CODE_LEN && (strcmp(buff,"FNEW0")==0 || strcmp(buff,"FNEW1")==0) ){	
							if(strcmp(buff,"FNEW0")==0)		
								dir=0;
							else
								dir=1;
							printf("Ricevuto messaggio '%s' da un peer vicino, avvio un nuovo flooding\n", buff);
							//ricezione della lunghezza del messaggio con la entry del daily register
							do{
								send_ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(send_ret<sizeof(uint16_t));
							//ricezione della entry del daily register
							do{
								send_ret=recv(i, buff, ntohs(msglen), 0);
							}while(send_ret<ntohs(msglen));
							sscanf(buff, "%c%d%c%d%c%d%c%d%c", &new_type, &new_quantity, &app, &day,&app, &month, &app, &year, &app);
							//inoltro della nuova entry
							keep_flooding(dir, new_type, new_quantity, day, month, year);
						}
						//il messaggio ricevuto è una richiesta di flooding di un registro giornaliero
						if(ret==CODE_LEN && (strcmp(buff,"FREQ0")==0 || strcmp(buff,"FREQ1")==0) ){	
							if(strcmp(buff,"FREQ0")==0)		
								dir=0;
							else
								dir=1;
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							//controllo se è presente il registro, se è più aggiornato, se è chiuso ed eventualmente inoltro la richiesta di flooding
							check_register_req(dir, i);
						}
						//il messaggio ricevuto è una risposta ad una richiesta di un registro giornaliero
						if(ret==CODE_LEN && (strcmp(buff,"FANS0")==0 || strcmp(buff,"FANS1")==0) ){	
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							//Ricezione della lunghezza del messaggio di risposta
							do{
								send_ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(send_ret<sizeof(uint16_t));
							//ricezione della risposta
							do{
								ret=recv(i, buff, ntohs(msglen), 0);
							}while(ret<ntohs(msglen));
							sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &num_port, &app);
							printf("Ricevuta risposta alla richiesta del registro del giorno %d/%d/%d\n", day, month, year);
							//devo inoltrare la risposta a un altro peer perchè non è destinata a me
							if(strcmp(buff,"FANS0")==0)		
								dir=0;
							else
								dir=1;
							//inoltro della risposta
							send_answer_flood(dir, day,month, year,tot_tamponi,tot_casi, num_port);						
						}
						//il messaggio ricevuto è la richiesta di un vicino di mandare il proprio daily register
						if(ret==CODE_LEN && strcmp(buff,"GETDR")==0){
							send_daily_register(i);
						}
						//chiusura del socket di comunicazione, a meno che non sia quello usato per il server
						if(i!=server_sock){
							close(i);
							// Rimuovo il descrittore del socket dal set master
							FD_CLR(i, &master); 
							//rimozione del descrittore dal set locale
							FD_CLR(i, &local_master); 
						}
					}
				}	
			}
		} // Fine for f1
	}

}

//funzione per la corretta terminazione di un peer
void stop(){
	printf("Richiesta di terminazione del peer\n");
	send_stop_request();
	return;
};