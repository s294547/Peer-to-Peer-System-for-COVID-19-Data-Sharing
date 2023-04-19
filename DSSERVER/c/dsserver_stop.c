#include "../h/dsserver_data.h"
#include "../h/dsserver_flood.h"
#include "../h/dsserver_start.h"

//funzione per la rimozione e la deallocazione del peer
void rimuovi_peer(int peer_sock){
	struct Peer* appoggio=lista_peer;
	struct Peer* peer_prec=NULL;

	while(appoggio!=NULL){
		//lo individuo utilizzando l'identificatore del socket di comunicazione col peer
		if(peer_sock==appoggio->tcp_sv_com){
			//se il peer è in testa
			if(appoggio==lista_peer){
				//levo il peer dalla lista
				lista_peer=appoggio->next_peer;
				//aggiorno i vicini del nuovo peer in testa
				if(lista_peer!=NULL){
					if(appoggio->neighbor_1!=NULL){
						update_neigh(appoggio->neighbor_1->port);
						//notifico il peer dei suoi nuovi vicini
						send_neighs(appoggio->neighbor_1->port, -1);
					}
					if(appoggio->neighbor_2!=NULL){
						update_neigh(appoggio->neighbor_2->port);
						//notifico il peer dei suoi nuovi vicini
						send_neighs(appoggio->neighbor_2->port, -1);
					}
				}
			}
			//se non è in testa
			else{
				//levo il peer dalla lista
				peer_prec->next_peer=appoggio->next_peer;
				//aggiorno i vicini del peer, se ci sono
				if(appoggio->neighbor_1!=NULL){
					update_neigh(appoggio->neighbor_1->port);
					//notifico il peer dei suoi nuovi vicini
					send_neighs(appoggio->neighbor_1->port, -1);
				}
				if(appoggio->neighbor_2!=NULL){
					update_neigh(appoggio->neighbor_2->port);
					//notifico il peer dei suoi nuovi vicini
					send_neighs(appoggio->neighbor_2->port, -1);
				}
			}			
			//dealloco il peer
			free(appoggio);
			return;
		}
		//scorrimento della lista
		peer_prec=appoggio;
		appoggio=appoggio->next_peer;	
	}
}

//funzione per l'ottenimento dei registri da un peer che sta uscendo, se è l'ultimo peeer connesso al server
void ottieni_registri_ultimo_peer(int peer_sock){
	int ret, day, month, year, tot_tamponi, tot_casi, closed, num_reg, num_aggr, day2, month2, year2, data;
	uint16_t msglen;
	char buff[1024];
	char app, type, aggrt; 
	FILE* file_saved;
	time_t rawtime;
	struct tm* timeinfo;
	//ottengo l'ora locale
	time(&rawtime);
	// Converto l'ora
	timeinfo = localtime(&rawtime);
	//recupero dei nome del file (server(numero_server))
	char filename[12];
	sprintf(filename, "%s%d", "server", server_port);
	//apertura del file in lettura
	file_saved=fopen(filename, "w");
	//ricevo la lunghezza del messaggio contenente il numero di registri
	do{
		ret = recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while (ret<sizeof(uint16_t));
	//ricezione del numero dei registri
	do{
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff,"%d%c", &num_reg,&app);
	//scrivo il numero di registri da ricevere su file
	fprintf(file_saved, "%d%c", num_reg, app);
	//ciclo per la ricezione dei registri dell'ultimo peer
	while(num_reg>0){
		//ricezione della lunghezza del messaggio contenente il registro
		do{
			ret = recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while (ret<sizeof(uint16_t));
		//ricezione del registro
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
		sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &closed, &app);
		//se il giorno è precedente al giorno o è il giorno corrente ma siamo oltre le 18, chiudo il registro
		if((day<timeinfo->tm_mday && (month-1)==timeinfo->tm_mon && year==(timeinfo->tm_year+1900))|| ((month-1)<timeinfo->tm_mon && year==(timeinfo->tm_year+1900)) || (year<(timeinfo->tm_year+1900)) || ( timeinfo->tm_hour>=18 && day==timeinfo->tm_mday && (month-1)==timeinfo->tm_mon && year==(timeinfo->tm_year+1900)))
			closed=1;
		//scrivo le entry sul file
		fprintf(file_saved, "%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, closed, app);
		num_reg--;
	}
	//chiusura del file
	if(file_saved!=NULL)
		fclose(file_saved);
	//salvataggio dei dati aggregati su file
	sprintf(filename, "%s%d", "svaggr", server_port);
	//apertura del file il scrittura
	file_saved=fopen(filename, "w");
	//ricezione della lunghezza del messaggio contenente il numero di dati aggregati
	do{
		ret = recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while (ret<sizeof(uint16_t));
	//ricezione del numero dei dati aggregati
	do{
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff,"%d%c", &num_aggr,&app);
	//scrittura del numero di righe del file all'inizio del file stesso
	fprintf(file_saved, "%d%c", num_aggr, app);
	//ciclo per la ricezione dei dati aggregati dell'ultimo peer
	while(num_aggr>0){
		//ricezione della lunghezza del messaggio contenente il dato aggregato
		do{
			ret = recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while (ret<sizeof(uint16_t));
		//ricezione del dato aggregato
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
		sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &day2, &app, &month2, &app, &year2, &aggrt, &ret, &type, &data,  &app);
		//scrivo le entry sul file
		fprintf(file_saved, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, day2, app, month2, app, year2, aggrt, ret, type, data, app);
		num_aggr--;
	}
	//chiusura del file
	if(file_saved!=NULL)
		fclose(file_saved);
	//comunico al peer che ho ricevuto tutti i suoi registri e dati aggregati
	strncpy(buff, "SRACK", 5);
	buff[5]='\0';
	do{
		ret=send(peer_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
}


//funzione per la terminazione di un peer
void stop_peer(int peer_sock){
	char buff[1024];
	int ret;
	//se il peer uscente è l'ultimo, ottengo tutti i suoi registri, da dare al prossimo primo peer, altrimenti aggiorno i vicini
	while(1){
		do{
			ret=recv(peer_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		//ottengo l'ordine di aggiornare i vicini dei vicini del peer uscente
		if(strcmp(buff, "STNGH")==0){
			//deallocazione del peer
			rimuovi_peer(peer_sock);
			//lo informo di aver inviato i vicini ai suoi vicini
			strncpy(buff,"NGEND", 5);
			buff[5]='\0';
			do{
				ret = send(peer_sock, buff, CODE_LEN, 0);
			}while(ret<CODE_LEN);
			break;
		}
		//ottenimento dei registri dall'ultimo peer
		else{
			if(strcmp(buff, "STREG")==0)
				ottieni_registri_ultimo_peer(peer_sock);
		}
	}
	//informo il peer che ho terminato di fare ciò che dovevo
	strncpy(buff, "ENDOP", 5);
	buff[5]='\0';
	do{
		ret=send(peer_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	//ricezione del messaggio 'STEND' da parte del peer, STOP del peer terminata
	do{
		ret=recv(peer_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	if(strcmp(buff, "STEND")==0)
		return;
	//chiusura del socket di comunicazione col peer
	close(peer_sock);
	//rimozione del peer dal master set
	FD_CLR(peer_sock, &master);
}


//funzione per inviare a ciascun peer una richiesta di terminazione da parte del server
int send_esc_to_peers(){
	struct Peer* appoggio=lista_peer;
	int tot_peer=0, send_ret;
	char buff[32];
	//mando un messaggio di "SVESC" a tutti i peer per richiederne la terminazione
	while(appoggio!=NULL){
		strncpy(buff,"SVESC",5);
		buff[5]='\0';
		do{	
			send_ret=send(appoggio->tcp_sv_com, buff, CODE_LEN, 0);
		}while(send_ret<CODE_LEN);
		//incremento il numero di peer
		tot_peer++;
		//prossimo peer
		appoggio=appoggio->next_peer;
	}
	return tot_peer;
}

//funzione per la terminazione del dsserver e la conseguente terminazione di tutti i suoi peer
void esc(){
	int i=0, ret=0, k=0, send_ret, local_fdmax=fdmax, tot_peer;
	fd_set read_fds; /* Set di lettura gestito dalla select */
	fd_set local_master; //set  master 'locale',non sono accettate richieste sul socket udp o comandi da terminale
	char buff[1024]; // Buffer
	/* Azzero i set */
	FD_ZERO(&local_master);
	FD_ZERO(&read_fds);
	local_master=master;
	//Rimuovo il socket udp dal set
	FD_CLR(server, &local_master);
	//Rimuovo stdin dal set
	FD_CLR(0, &local_master);
	// Tengo traccia del maggiore
	local_fdmax = fdmax;
	//invio richiesta di esc a tutti i peer, utile se un peer non ha già richiesto una esc
	tot_peer=send_esc_to_peers();
	//ciclo effettuato finchè tutti i peer non escono
	while(tot_peer>0){
		k++;
		read_fds = local_master; // read_fds sarà modificato dalla select
		select(local_fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=local_fdmax; i>=0; i--) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto
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
							send_ret=send(i, buff, CODE_LEN, 0);
						}while(send_ret<CODE_LEN);
						stop_peer(i);
						tot_peer--;
						FD_CLR(i, &local_master);
					}
				}			
			}
		} // Fine for f1
	} // Fine for(;;)
	exit(0);
};