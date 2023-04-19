#include "../h/peerdata.h"
#include "../h/peerutilfunc.h"
#include "../h/peer_stop.h"

//funzione per il floodig di una nuova entry
void flood_new_data(char type, int quantity, int dayf, int monthf, int yearf){
	fd_set local_master; //set locale, utilizzato per accettare nuove richieste da peer (FNEW0,FNEW1,STREG, RAGGR, FREQ0, FREQ1, FANS0,FANS1,GETDR) o dal server (FLACK, NEIGH, SVESC)
	fd_set read_fds; // Set di lettura gestito dalla select 
	struct sockaddr_in sock_addr;
	int local_fdmax; // Numero max di descrittori
	int ret=0, send_ret=0, i, addrlen, newfd, dir, stop_req=0, day, month, year, tot_tamponi, tot_casi, num_port;
	uint16_t msglen;
	char new_type;
	int new_quantity;
	char buff[1024], app=' ';
	//invio 'FLNEW' al server: una volta ricevuto, il server non accetterà fino al termine del flooding nuovi comandi di START o STOP
	strncpy(buff,"FLNEW",5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato messaggio 'FLNEW' al server per richiedere un nuovo flooding\n");
	//il local maste sarà pari al master, senza però stdin nel set
	local_master=master;
	FD_CLR(0, &local_master);
	FD_ZERO(&read_fds);
	// Tengo traccia del maggiore 
	local_fdmax =fdmax;
	for(;;){
		read_fds = local_master; // read_fds sarà modificato dalla select
		select(local_fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=local_fdmax; i>=0; i--) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto
				if(i == peer_sock) { // se il peer riceve una richiesta di connessione 
					// se è stata chiamata la funzione start
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
						//se ricevo una richiesta di uscire da parte del server, esco quando ho terminato il flooding
						if(ret==CODE_LEN && strcmp(buff,"SVESC")==0){
							stop_req=1;
						}
						//se il messaggio ricevuto è un FLACK posso iniziare il flooding
						if(ret==CODE_LEN && strcmp(buff,"FLACK")==0){
							printf("Ricevuto messaggio 'FLACK' dal server, avvio un nuovo flooding\n");
							//flooding al vicino a 'sinistra'
							start_flooding(0, type, quantity, dayf, monthf, yearf);
							//flooding al vicino a 'destra'
							start_flooding(1, type, quantity, dayf, monthf, yearf);
							//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminato il flooding
							if(stop_req){
								stop();
								exit(0);
							}
							return;
						}
						//se il messaggio ricevuto è per l'aggiunta di nuovi vicini
						if(ret==CODE_LEN && strcmp(buff,"NEIGH")==0){
							aggiorna_vicini(i);
							//il precedente messaggio 'FLNEW' è stato perso dal server, lo devo rinviare
							strncpy(buff,"FLNEW",5);
							buff[5]='\0';
							do{
								ret=send(server_sock, buff, CODE_LEN, 0);
							}while(ret<CODE_LEN);
							printf("Inviato messaggio 'FLNEW' al server per richiedere un nuovo flooding\n");
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
							//ricezione della lunghezza del messaggio con la entry
							do{
								send_ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(send_ret<sizeof(uint16_t));
							//ricezione della entry del daily register
							do{
								send_ret=recv(i, buff, ntohs(msglen), 0);
							}while(send_ret<ntohs(msglen));
							sscanf(buff, "%c%d%c%d%c%d%c%d%c", &new_type, &new_quantity, &app, &day, &app,  &month, &app, &year, &app);
							//inoltro della nuova entry e aggiornamento dei registri giornalieri
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
							//ricezione della lunghezza del messaggio di risposta
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
							//inoltro della risposta in flooding
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

//funzione per l'aggiunta di una nuova antry al daily register
void add(char type, int quantity){
	int day, month, year;
	struct daily_register* appoggio=registers;
	struct daily_register* new_register;
	time_t rawtime;
	struct tm* timeinfo;
	//ottengo l'ora locale
	time(&rawtime);
	// Converto l'ora
	timeinfo = localtime(&rawtime);
	//inizializzo i valori con giorno, mese ed anno corrente
	day=timeinfo->tm_mday, month=timeinfo->tm_mon+1 , year=timeinfo->tm_year+1900;
	//se l'orario corrente è oltre le 18, non posso aggiungere nulla al daily register  perchè è chiuso, devo inserire nei registri del giorno successivo
	if(timeinfo->tm_hour>=18){
		printf("Il registro del giorno corrente è chiuso, i dati sono inseriti nel registro del giorno successivo!\n");
		day=get_next_day(day, month, year);
		month=get_next_month(day, month);
		year=get_next_year(day, month, year);
	}
	
	//controllo se è presente il daily register
	while(appoggio!=NULL){
		if(appoggio->day==day && appoggio->month==month && appoggio->year==year){
			break;
		}
		else
			appoggio=appoggio->next_register;
	}
	//se non è presente, lo creo
	if(appoggio==NULL){
		new_register=create_daily_register(day, month, year);
	}
	else
		new_register=appoggio;
	//invio del dato a tutti i peer tramite flooding
	flood_new_data(type, quantity, day, month, year);
	//aggiorno il daily register con la nuova entry e modifico i dati totali
	update_with_entry(new_register, type, quantity);
};

void controllo_add(char* comando){
	//funzione per il controllo del formato degli argomenti del comando 'add' sia corretto
	int i=0, index=0, num;
	char type;
	char startnum[64];
	//dopo il comando deve esserci almeno uno spazio
	if(comando[3]!=' '){
		printf("Formato non valido!\n");
		return;
	}
	//dopo il comando possono esserci solo spazi o il tipo
	for(i=4; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		if(comando[i]=='T' || comando[i]=='N'){
			type=comando[i];
			i++;
			break;
		}
		printf("Formato del tipo non valido!\n");
		return;
	}
	//dopo il tipo, deve esserci uno spazio
	if(comando[i]!=' '){
		printf("Formato non valido!\n");
		return;
	}	
	//dopo il tipo possono esserci solo spazi o un valore numerico per la quantità
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		if(comando[i]<=57 && comando[i]>=48){
			break;
		}
		printf("Formato non valido!\n");
		return;
	}
	//controllo che per la quantità siano stati inseriti solo valori numerici
	while(comando[i]!=' ' && comando[i]!='\0'){
		//startnum come stringa di appoggio per ricavare il valore della quantità
		startnum[index]=comando[i];
		if(comando[i]>57 || comando[i]<48){
			printf("Formato non valido!\n");
			return;
		}
		i++;		
		index++;	
	}
	startnum[index]='\0';
	num=atoi(startnum);
	//dopo la quantità possono essere stati inseriti solo spazi
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		printf("Formato non valido!\n");
		return;
	}
	add(type,num);
}