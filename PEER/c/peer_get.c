#include "../h/peerdata.h"
#include "../h/peerutilfunc.h"
#include "../h/peer_stop.h"

//funzione per inviare la notifica della terminazione del flooding da un lato
void send_end_to_server(int i){
	char buff[1024];
	int ret;
	//invio del messaggio FLEND
	strncpy(buff,"FLEND", 5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	if(i==3)
		printf("Inviato il messaggio 'FLEND' al server, ho ricevuto una risposta alla richiesta del registro giornaliero\n");
	else
		printf("Inviato il messaggio 'FLEND' al server, non è presente il vicino %s a cui fare la richiesta\n", (i==0)?"sinistro":"destro");
}

//funzione per il calcolo del totale di tamponi  casi dal giorno day1/month1/year1 al giorno day2/month2/year2
struct data_aggr* calcola_totale(int day1, int month1, int year1, int day2, int month2, int year2, char type){
	struct daily_register* appoggio;
	struct data_aggr* new_aggr=NULL;
	int day=day1, month=month1, year=year1;
	int tot=0;
	//scorro tutti i giorni compresi tra day1/month1/year1 e day2/month2/year2
	while(1){
		//cerco il registro del giorno
		appoggio=find_daily_register(day, month, year);
		//aggiorno il totale
		tot+=(type=='T')?appoggio->tot_tamponi:appoggio->tot_casi;
		if(day==day2 && month==month2 && year==year2)
			break;
		//trovo il giorno successivo
		day=get_next_day(day, month, year);
		month=get_next_month(day, month);
		year=get_next_year(day, month, year);
	}	
	//creazione del nuovo dato aggregato
	new_aggr=create_aggr(day1, month1, year1, day2, month2, year2, 'T', type, tot);
	printf("Calcolcato il totale dei %s dal giorno %d/%d/%d al giorno %d/%d/%d, valore=%d\n", (type=='T')?"tamponi":"casi", day1, month1, year1, day2, month2, year2, tot);
	return new_aggr;
}

//funzione per il calcolo della variazione di tamponi dal giorno day1/month1/year1 al giorno day2/month2/year2
struct data_aggr* calcola_variazione(int day1, int month1, int year1, int day2, int month2, int year2, char type){
	struct daily_register* appoggio, *appoggiobis;
	struct data_aggr* new_aggr=NULL;
	int var=0, data1, data2;
	//trovo i registri dei giorni
	appoggio=find_daily_register(day1, month1, year1);
	appoggiobis=find_daily_register(day2, month2, year2);
	//trovo i dati relativi ai due giorni
	data1=(type=='T')?appoggio->tot_tamponi:appoggio->tot_casi;
	data2=(type=='T')?appoggiobis->tot_tamponi:appoggiobis->tot_casi;
	//calcolo della variazione
	var=data2-data1;
	//creazione del dato aggregato
	new_aggr=create_aggr(day1, month1, year1, day2, month2, year2, 'V', type, var);
	printf("Calcolcata la variazione dei %s dal giorno %d/%d/%d al giorno %d/%d/%d, valore=%d\n", (type=='T')?"tamponi":"casi", day1, month1, year1, day2, month2, year2, var);
	return new_aggr;
}

//funzione per la ricezione di un dato aggregato precedentemente richiesto ad un vicino
struct data_aggr* get_neigh_aggr(int i){
		struct data_aggr* new_aggr;
		int day1, month1, year1, day2, month2, year2, data, ret;
		uint16_t msglen;
		char app, type, aggrt;
		char buff[1024];
		//ricezione della lunghezza del messaggio con il dato aggregato
		do{
			ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//ricezione del dato aggregato dal vicino
		do{
			ret=recv(i, buff, ntohs(msglen), 0);
		}while(ret<ntohs(msglen));
		sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day1,&app, &month1,&app, &year1,&app, &day2,&app, &month2,&app, &year2, &aggrt,&ret, &type, &data, &app);
		printf("Ricevuto il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al %d/%d/%d \n", aggrt, type, day1, month1, year1, day2, month2, year2);
		//controllo che prima il dato aggregato non sia arrivato da un altro vicino
		new_aggr=find_aggr(day1, month1, year1, day2, month2, year2, aggrt, type);
		//se il dato non era arrivato da un altro vicino, lo creo
		if(new_aggr==NULL)
			new_aggr=create_aggr(day1, month1,year1, day2, month2, year2, aggrt, type, data);
		//chiudo il socket per la comunicazione con quel peer
		close(i);
		return new_aggr;
}

//funzione per la richiesta e la ricezione il flooding di un registro
void get_updated_register(int day, int month, int year, struct daily_register* appoggio){
	fd_set local_master; //set locale, utilizzato per accettare nuove richieste da peer (FNEW0, FNEW1, FREQ0, FREQ1, FANS0, FANS1, GETDR, STREG, RAGGR) o dal server (FLACK, NEIGH, SVESC)
	fd_set read_fds; /* Set di lettura gestito dalla select */
	struct sockaddr_in sock_addr;
	int local_fdmax; // Numero max di descrittori
	int ret=0, send_ret=0, i, addrlen, newfd, dir, stop_req=0, arrived_answers=0, tot_tamponi, tot_casi, num_port, dayf, monthf, yearf;
	uint16_t msglen;
	char new_type;
	int new_quantity;
	char buff[1024], app=' ';
	//invio 'FLNEW' al server: una volta ricevuto, il server non accetterà fino al termine del flooding nuovi comandi si START o STOP
	strncpy(buff,"FLNEW",5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato messaggio 'FLNEW' al server per richiedere il registro in flooding\n");
	//il set local_master è pari al master, senza stdin
	local_master=master;
	FD_CLR(0, &local_master);
	FD_ZERO(&read_fds);
	// Tengo traccia del maggiore 
	local_fdmax = fdmax;
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
						//se ricevo una richiesta di uscire da parte del server, esco quando ho terminato la richiesta del dato ai vicini
						if(ret==CODE_LEN && strcmp(buff,"SVESC")==0){
							stop_req=1;
						}
						//se il messaggio ricevuto è un FLACK posso avviare il flooding della richiesta del registro
						if(ret==CODE_LEN && strcmp(buff,"FLACK")==0){
							printf("Ricevuto messaggio 'FLACK' dal server, avvio la richiesta del registro ai vicini\n");
							//flooding al vicino a 'sinistra'
							if(neigh[0]!=-1)
								req_register_flood(0, day, month, year, appoggio->tot_tamponi, appoggio->tot_casi, peer_id);
							else{
								arrived_answers++;
								send_end_to_server(0);
							}
							//flooding al vicino a 'destra'
							if(neigh[1]!=-1)
								req_register_flood(1, day, month, year, appoggio->tot_tamponi, appoggio->tot_casi, peer_id);
							else{
								arrived_answers++;
								send_end_to_server(1);
							}
							//se non erano presenti vicini, ho terminato
							if(arrived_answers==2){
								//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminata la richiesta
									if(stop_req){
										stop();
										exit(0);
									}
									return;
							}

							break;
						}
						//se il messaggio ricevuto è per l'aggiunta di nuovi vicini
						if(ret==CODE_LEN && strcmp(buff,"NEIGH")==0){
							aggiorna_vicini(i);
							//invio 'FLNEW' al server: una volta ricevuto, il server non accetterà fino al termine del flooding nuovi comandi si START o STOP
							strncpy(buff,"FLNEW",5);
							buff[5]='\0';
							do{
								ret=send(server_sock, buff, CODE_LEN, 0);
							}while(ret<CODE_LEN);
							printf("Inviato messaggio 'FLNEW' al server per richiedere il registro in flooding\n");
							continue;
						}
					}
					//se a mandare il messaggio è stato un altro peer
					if(i!=0){
						//richiesta da parte di un vicino di un dato aggregato
						if(ret==CODE_LEN && strcmp(buff, "RAGGR")==0){
								send_aggr(i);
						}
						//il messaggio ricevuto è una richiesta di flooding di un registro giornaliero
						if(ret==CODE_LEN && (strcmp(buff,"FREQ0")==0 || strcmp(buff,"FREQ1")==0) ){	
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							if(strcmp(buff,"FREQ0")==0)		
								dir=0;
							else
								dir=1;
							//controllo se è presente il registro, se è più aggiornato, se è chiuso ed eventualmente inoltro la richiesta di flooding
							check_register_req(dir, i);
						}
						//il messaggio ricevuto è una risposta ad una richiesta di un registro giornaliero
						if(ret==CODE_LEN && (strcmp(buff,"FANS0")==0 || strcmp(buff,"FANS1")==0) ){	
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							//ricezione della lunghezza del messaggio di risposta
							do{
								ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(ret<sizeof(uint16_t));
							//ricezione della risposta
							do{
								ret=recv(i, buff, ntohs(msglen), 0);
							}while(ret<ntohs(msglen));
							sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &num_port, &app);
							printf("Ricevuta risposta alla richiesta del registro del giorno %d/%d/%d\n", day, month, year);
							//se la richiesta proviene dal peer in esecuzione
							if(num_port==peer_id){
								//aggiornamento dei dati se necessario
								appoggio->tot_tamponi=(tot_tamponi>appoggio->tot_tamponi)?tot_tamponi:appoggio->tot_tamponi;
								appoggio->tot_casi=(tot_casi>appoggio->tot_casi)?tot_casi:appoggio->tot_casi;
								//incremento delle risposte arrivate
								arrived_answers++;
								//mando "FLEND" al server
								send_end_to_server(3);
								// se le risposte sono tutte quante arrivate, termino il flooding
								if(arrived_answers==2){

								//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminata la richiesta
									if(stop_req){
										stop();
										exit(0);
									}
									return;
								}
							}
							//se devo inoltrare la risposta a un altro peer perchè non è destinata a me
							else{
								if(strcmp(buff,"FANS0")==0)		
									dir=0;
								else
									dir=1;
								//inoltro della risposta
								send_answer_flood(dir, day,month, year,tot_tamponi,tot_casi, num_port);
							}
						}
						//richiesta di un peer vicino di mandare i propri registri
						if(ret==CODE_LEN && strcmp(buff,"STREG")==0){
							printf("Ricevuto messaggio 'STREG' di un vicino, avvio la routine per ricevere i suoi registri\n");
							//salvataggio su file dei registri giornalieri
							ottieni_registri_aggr_vicino(i, 0);
						}
						//il messaggio ricevuto è una richiesta di flooding di una nuova entry del daily register
						if(ret==CODE_LEN && (strcmp(buff,"FNEW0")==0 || strcmp(buff,"FNEW1")==0) ){	
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							if(strcmp(buff,"FNEW0")==0)		
								dir=0;
							else
								dir=1;
							//ricezione della lunghezza del messaggio
							do{
								ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(ret<sizeof(uint16_t));
							//ricezione della entry del daily register
							do{
								send_ret=recv(i, buff, ntohs(msglen), 0);
							}while(send_ret<ntohs(msglen));
							sscanf(buff, "%c%d%c%d%c%d%c%d%c", &new_type, &new_quantity, &app, &dayf,&app, &monthf, &app, &yearf, &app);
							//inoltro della nuova entry
							keep_flooding(dir, new_type, new_quantity, dayf, monthf, yearf);
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

//funzione per la richiesta di un dato aggregato al server
int ask_req_aggr(int i, int day1, int month1, int year1, int day2, int month2, int year2, char aggrt, char type){
	struct sockaddr_in neigh_addr;
	int neigh_len, ret;
	uint16_t msglen;
	//socket usato per la connessione al vicino
	int peer_cl_sock;
	char buff[1024], app=' ';
	//creazione dell'indirizzo del vicino
	memset(&neigh_addr, 0, sizeof(neigh_addr));
	neigh_len=sizeof(neigh_addr);
	neigh_addr.sin_family = AF_INET;
	inet_pton(AF_INET, peer_ip , &neigh_addr.sin_addr);
	neigh_addr.sin_port = htons(neigh[i]);
	
	//se non c'è il vicino,lo segnalo al server  (fine 'flooding' da un lato)
	if(neigh[i]==-1){
		strncpy(buff,"FLEND", 5);
		buff[5]='\0';
		do{
			ret=send(server_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio 'FLEND' al server, non è presente il vicino %s a cui fare la richiesta\n", (i==0)?"sinistro":"destro");	
		return -1;
	}
	//se c'è il vicino, invio la richiesta del dato aggregato
	else{
		//creazione del socket per la comunicazione con il vicino e connessione ad esso
		peer_cl_sock=socket(AF_INET, SOCK_STREAM, 0);
		printf("Connessione al vicino di porta: %d per la richiesta di un dato aggregato\n", neigh[i]);
		do{
			ret=connect(peer_cl_sock, (struct sockaddr*)&neigh_addr, neigh_len);
		}
		while(ret<0);
		
		//invio della richiesta del dato aggregato al vicino
		strncpy(buff,"RAGGR", 5);
		buff[5]='\0';
		do{
			ret=send(peer_cl_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio 'RAGGR' al vicino di porta:%d\n", neigh[i]);
		//creazione del messggio con il dato aggregato richiesto
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c", day1,app, month1,app, year1,app, day2,app, month2,app, year2,aggrt,ret, type);
		//invio della lunghezza del messaggio con il dato aggregato
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio al vicino il dato aggregato richiesto
		do{
			ret=send(peer_cl_sock, buff, (strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		printf("Inviata una richiesta del dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al %d/%d/%d al vicino di porta:%d\n", aggrt, type, day1, month1, year1, day2, month2, year2, neigh[i]);
		return peer_cl_sock;
	}
}

//funzione per la richiesta e l'eventuale ricezione di dati aggregati dai vicini
struct data_aggr* ask_aggr_to_neigh(int day1, int month1, int year1, int day2, int month2, int year2, char aggrt, char type){
	fd_set local_master; //set locale, utilizzato per accettare nuove richieste da peer (FNEW0, FNEW1,RAGGR, NAGGR, FAGGR, FREQ0, FREQ1, FANS0, FANS1, GETDR, STREG) o dal server (FLACK, NEIGH, SVESC)
	fd_set read_fds; /* Set di lettura gestito dalla select */
	struct data_aggr* new_aggr=NULL; //dato aggregato che riceverò dai vicini, se presente
	struct sockaddr_in sock_addr;
	uint16_t msglen;
	int local_fdmax; // Numero max di descrittori
	int ret=0, send_ret=0, i, addrlen, newfd, dir, stop_req=0, neighsock1, neighsock2, day, month, year, tot_tamponi, tot_casi, num_port;
	char new_type;
	int new_quantity;
	char buff[1024], app=' ';
	//invio 'FLNEW' al server: una volta ricevuto, il server non accetterà fino al termine del flooding nuovi comandi si START o STOP
	strncpy(buff,"FLNEW",5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato messaggio 'FLNEW' al server per richiedere il dato aggregato ai vicini\n");
	//local_set pari al master senza stdin
	local_master=master;
	FD_CLR(0, &local_master);
	FD_ZERO(&read_fds);
	// Tengo traccia del maggiore 
	local_fdmax = fdmax;
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
						//se ricevo una richiesta di uscire da parte del server, esco quando ho terminato la richiesta del dato ai vicini
						if(ret==CODE_LEN && strcmp(buff,"SVESC")==0){
							stop_req=1;
						}
						//se il messaggio ricevuto è un FLACK posso richiedere il dato ai vicini
						if(ret==CODE_LEN && strcmp(buff,"FLACK")==0){
							printf("Ricevuto messaggio 'FLACK' dal server, avvio la richiesta del dato aggregato ai vicini\n");
							//richiesta al vicino a 'sinistra'
							neighsock1=ask_req_aggr(0, day1, month1, year1, day2, month2, year2, aggrt, type);
							if(neighsock1!=-1){
								FD_SET(neighsock1, &local_master);
								if(neighsock1 > local_fdmax){ local_fdmax = neighsock1;}
							}
							//richiesta al vicino a 'destra'
							neighsock2=ask_req_aggr(1, day1, month1, year1, day2, month2, year2, aggrt, type);
							if(neighsock2!=-1){
								FD_SET(neighsock2, &local_master);
								if(neighsock2 > local_fdmax){ local_fdmax = neighsock2;}
							}
							//se non era presente nessuno dei due vicini, termino
							if(!FD_ISSET(neighsock1, &local_master) && !FD_ISSET(neighsock2, &local_master)){
								//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminata la richiesta
									if(stop_req){
										stop();
										exit(0);
									}
									return new_aggr;
							}

							break;
						}
						//se il messaggio ricevuto è per l'aggiunta di nuovi vicini
						if(ret==CODE_LEN && strcmp(buff,"NEIGH")==0){
							aggiorna_vicini(i);
							//invio 'FLNEW' al server: una volta ricevuto, il server non accetterà fino al termine del flooding nuovi comandi si START o STOP
							strncpy(buff,"FLNEW",5);
							buff[5]='\0';
							do{
								ret=send(server_sock, buff, CODE_LEN, 0);
							}while(ret<CODE_LEN);
							printf("Inviato messaggio 'FLNEW' al server per richiedere il dato aggregato ai vicini\n");
							continue;
						}
					}
					//se a mandare il messaggio è stato un altro peer
					if(i!=0){
						//richiesta da parte di un vicino di un dato aggregato
						if(ret==CODE_LEN && strcmp(buff, "RAGGR")==0){
								send_aggr(i);
						}
						//risposta di un vicino alla richiesta del dato aggregato, il dato aggregato non è presente
						if(ret==CODE_LEN && strcmp(buff, "NAGGR")==0){
								printf("Uno dei vicini non ha il dato aggregato richiesto\n");
								//chiusura del socket di comunicazione con il vicino
								close(i);
								//rimuovo il socket dal set locale
								FD_CLR(i, &local_master);
								//se ho ricevuto risposta alla mia richiesta di dato aggregato da entrambi i vicini
								if(!FD_ISSET(neighsock1, &local_master) && !FD_ISSET(neighsock2, &local_master)){
								//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminata la richiesta
									if(stop_req){
										stop();
										exit(0);
									}
									return new_aggr;
								}
						}
						//risposta di un vicino alla richiesta del dato aggregato, il dato aggregato era presente
						if(ret==CODE_LEN && strcmp(buff, "FAGGR")==0){
								//ottenimento del dato aggregato
								new_aggr=get_neigh_aggr(i);
								//rimuovo il socket dal set locale
								FD_CLR(i, &local_master);
								//se ho ricevuto risposta alla mia richiesta di dato aggregato da entrambi i vicini
								if(!FD_ISSET(neighsock1, &local_master) && !FD_ISSET(neighsock2, &local_master)){
								//se era stato ricevuto un messaggio SVESC da parte del server, esco una volta terminata la richiesta
									if(stop_req){
										stop();
										exit(0);
									}
									return new_aggr;
								}
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

//funzione per cercare, richiedere ed eventualemente calcolare il dato aggregato di tipo aggrt sui dati di tipo type dal giorno day1/month1/year1 al giorno day2/month2/year2
void trova_dato(int day1, int month1, int year1, int day2, int month2, int year2, char aggrt, char type){
	struct daily_register* appoggio;
	struct data_aggr* new_aggr=NULL;
	int day=day1, month=month1, year=year1, tot_days=0, tot_aggr, nxtday, nxtmonth, nxtyear ;

	//Se è richiesto il calcolo di un totale
	if(aggrt=='T'){
		//controllo se ho già il dato aggregato
		new_aggr=find_aggr(day1, month1, year1, day2, month2, year2, aggrt, type);
		if(new_aggr!=NULL){
			printf("Il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al giorno %d/%d/%d è già presente e vale %d\n", aggrt, type, day1, month1, year1, day2, month2, year2, new_aggr->data);
			return;
		}
		//controllo se uno dei vicini ha il dato aggregato
		new_aggr=ask_aggr_to_neigh(day1, month1, year1, day2, month2, year2, aggrt, type);
		if(new_aggr!=NULL){
			printf("Il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al giorno %d/%d/%d è stato ricevuto e vale %d\n", aggrt, type, day1, month1, year1, day2, month2, year2, new_aggr->data);
			return;
		}
		//se non ho il dato aggregato e nemmneo i vicini lo hanno, controllo di avere e nel caso richiedo tutti i registri necessari a calcolarlo
		while(1){
			appoggio=find_daily_register(day, month, year);
			//se il registro non è presente, lo creo vuoto e non chiuso
			if(appoggio==NULL)
				appoggio=create_register(day, month, year, 0, 0, 0);
	 		//se è presente ed è chiuso, continuo il ciclo
			if(appoggio->closed==1){
				//se era l'ultimo giorno, blocco il ciclo
				if(day==day2 && month==month2 && year==year2)
					break;
				day=get_next_day(day, month, year);
				month=get_next_month(day, month);
				year=get_next_year(day, month, year);
				continue;
			}
			//se non era presente si richiede il registro ad altri vicini, se era presente e non era chiuso si controlla se ce n'è uno più aggiornato
			get_updated_register(day, month, year, appoggio);
			//fine del cciclo
			if(day==day2 && month==month2 && year==year2)
				break;
			//giorno successivo
			day=get_next_day(day, month, year);
			month=get_next_month(day, month);
			year=get_next_year(day, month, year);
		}	
		new_aggr=calcola_totale(day1, month1, year1, day2, month2, year2, type);
	}
	//se è richeisto il calcolo di una o più variazioni
	else{
		//ciclo per trovare il totale dei giorni presenti nel periodo
		while(1){
			tot_days++;
			//fine del cciclo
			if(day==day2 && month==month2 && year==year2)
				break;
			day=get_next_day(day, month, year);
			month=get_next_month(day, month);
			year=get_next_year(day, month, year);
		}
		//numero di dati di calcolare, pari al numero di giorni compresi nel periodo meno 1
		tot_aggr=tot_days-1;
		//primo giorno su cui calcolare la variaziome
		day=day1;
		month=month1;
		year=year1;
		//giorno successivo
		nxtday=day;
		nxtmonth=month;
		nxtyear=year;
		nxtday=get_next_day(nxtday, nxtmonth, nxtyear);
		nxtmonth=get_next_month(nxtday, nxtmonth);
		nxtyear=get_next_year(nxtday, nxtmonth, nxtyear);
		//Calcolo di tutte le possibili differenze
		while(tot_aggr>0){
			//controllo se ho già il dato aggregato
			new_aggr=find_aggr(day, month, year, nxtday, nxtmonth, nxtyear, aggrt, type);
			if(new_aggr!=NULL){
				printf("Il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al giorno %d/%d/%d è già presente e vale %d\n", aggrt, type, day, month, year, nxtday, nxtmonth, nxtyear, new_aggr->data);
				//continuo il ciclo
				tot_aggr--;
				day=nxtday;
				month=nxtmonth;
				year=nxtyear;
				nxtday=get_next_day(nxtday, nxtmonth, nxtyear);
				nxtmonth=get_next_month(nxtday, nxtmonth);
				nxtyear=get_next_year(nxtday, nxtmonth, nxtyear);
				continue;
			}
			//controllo se la variazione è disponibile presso un vicino
			new_aggr=ask_aggr_to_neigh(day, month, year, nxtday, nxtmonth, nxtyear, aggrt, type);
			if(new_aggr!=NULL){
				printf("Il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al giorno %d/%d/%d è stato ricveuto e vale %d\n", aggrt, type, day, month, year, nxtday, nxtmonth, nxtyear, new_aggr->data);
				//continuo il ciclo
				tot_aggr--;
				day=nxtday;
				month=nxtmonth;
				year=nxtyear;
				nxtday=get_next_day(nxtday, nxtmonth, nxtyear);
				nxtmonth=get_next_month(nxtday, nxtmonth);
				nxtyear=get_next_year(nxtday, nxtmonth, nxtyear);
				continue;
			}
			// se devo calcolare il dato aggregato, controllo che siano presenti i daily register necessari
			appoggio=find_daily_register(day, month, year);
			//se il registro non è presente, lo creo vuoto e non chiuso
			if(appoggio==NULL)
				appoggio=create_register(day, month, year, 0, 0, 0);
			if(appoggio->closed==0)
			//se non era presente si richiede il registro ad altri vicini, se era presente e non era chiuso si controlla se ce n'è uno più aggiornato
				get_updated_register(day, month, year, appoggio);
			appoggio=find_daily_register(nxtday, nxtmonth, nxtyear);
			//se il registro non è presente, lo creo vuoto e non chiuso
			if(appoggio==NULL)
				appoggio=create_register(nxtday, nxtmonth, nxtyear, 0, 0, 0);
			if(appoggio->closed==0)
			//se non era presente si richiede il registro ad altri vicini, se era presente e non era chiuso si controlla se ce n'è uno più aggiornato
				get_updated_register(nxtday, nxtmonth, nxtyear, appoggio);
			//alla fine, il mio registro sarà quello più aggiornato 
			appoggio->closed=1;
			//calcolo della vaiazione
			new_aggr=calcola_variazione(day, month, year, nxtday, nxtmonth, nxtyear, type);
			//aggiornamento dei dati per il prossimo ciclo
			tot_aggr--;
			day=nxtday;
			month=nxtmonth;
			year=nxtyear;
			nxtday=get_next_day(nxtday, nxtmonth, nxtyear);
			nxtmonth=get_next_month(nxtday, nxtmonth);
			nxtyear=get_next_year(nxtday, nxtmonth, nxtyear);
		}
	}
}

//funzione per l'ottenimento di un dato aggregato
void get(char aggrt, char type, char* data1, char* data2){
	int giorno1, giorno2, mese1, mese2, anno1, anno2;
	time_t rawtime;
	struct tm* timeinfo;
	//per prelevare le stringhe di giorni, mesi, anni da convertire poi numericamente
	char appoggio[11];
	//se la data iniziale è specificata
	if(data1[0]!='*'){
		strncpy(appoggio, data1, 11);
		//inserisco i '\0' per ricavare le stringhe
		appoggio[2]=appoggio[5]='\0';
		giorno1=atoi(appoggio);
		mese1=atoi(appoggio+3);
		anno1=atoi(appoggio+6);
	}
	//se la data iniziale non è specificata, parto da febbraio 2020, quando sono stati rilevati i primi casi di covid in italia
	else{
		giorno1=1;
		mese1=2;
		anno1=2020;
	}
	//se la data finale è specificata
	if(data2[0]!='*'){
		strncpy(appoggio, data2, 11);
		appoggio[2]=appoggio[5]='\0';
		giorno2=atoi(appoggio);
		mese2=atoi(appoggio+3);
		anno2=atoi(appoggio+6);
	}
	//se non è specificata, se sono oltre le 18 uso il giorno corrente, se sono prima delle 18 scelgo quello del giorno precedente
	else{
		//ottengo l'ora locale
		time(&rawtime);
		// Converto l'ora
		timeinfo = localtime(&rawtime);
		if(timeinfo->tm_hour<18){
			giorno2=get_prev_day(timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
			mese2=get_prev_month(timeinfo->tm_mday, timeinfo->tm_mon+1);
			anno2=get_prev_year(timeinfo->tm_mday, timeinfo->tm_mon+1, timeinfo->tm_year+1900);
		}
		else{
			giorno2=timeinfo->tm_mday;
			mese2=timeinfo->tm_mon+1;
			anno2=timeinfo->tm_year+1900;
		}		
	}
	//controllo se il dato è già stato calcolato, se lo ha un vicino o lo calcolo
	trova_dato(giorno1, mese1, anno1, giorno2, mese2, anno2, aggrt, type);
}


void controllo_get(char* comando){
	//funzione per il controllo del formato degli argomenti del comando 'get' sia corretto
	int i=0, k=0, j=0, index=0,  num, data_num=0, ret=0;
	char aggr,type;
	char date_1[11], date_2[11];
	char startnum[64];
	//dopo il comando deve essere stato inserito almeno uno spazio
	if(comando[3]!=' '){
		printf("Formato non valido!\n");
		return;
	}
	//dopo il comando possono essere stati inseriti solo spazi o il valore aggr
	for(i=4; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		if(comando[i]=='T' || comando[i]=='V'){
			aggr=comando[i];
			i++;
			break;
		}
		printf("Formato non valido!\n");
		return;
	}
	//dopo il valore di aggr deve essere stato inserito almeno uno spazio
	if(comando[i]!=' '){
		printf("Formato non valido!\n");
		return;
	}	
	//dopo aggr possono essere stati inseriti solo spazi o il valore type
	for(; i<strlen(comando) ;i++){
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
	//dopo il valore di type deve essere stato inserito almeno uno spazio
	if(comando[i]!=' '){
		printf("Formato non valido!\n");
		return;
	}
	//dopo type possono essere stati inseriti solo spazi, un valore numerico se inizia la data o '*' se non si vuole specificare la data iniziale
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		if(comando[i]<=57 || comando[i]<=48 || comando[i]=='*'){
				break;
			}
		printf("Formato del tipo non valido!\n");
		return;
	}
	//inserisco la prima data in formato stringa su date_1
	strncpy(date_1, comando+i, 10);
	//ciclo per il controllo del formato delle date, data_num tiene conto di quale data sto controllando
	while(data_num<2){
		if(data_num==1){
			//inserisco la prima data in formato stringa su date_2
			strncpy(date_2, comando+i, 10);
		}
		do{
			//uso startnum per mantenere rispettivamente il valore del giorno, mese, anno in formato stringa
			startnum[index]=comando[i];
			//se è stato inserito '*' passo a controllare la data successiva o termino il controllo delle date
			if(comando[i]=='*' && k==0 && j==0){
				i++;
					if(data_num<1){
						//se è la prima data, deve essere seguito da '-'
						if(comando[i]!='-'){
							printf("Formato non valido!\n");
							return;
						}
						date_1[1]='\0';
						i++;
					}
					else
						date_2[1]='\0';
				break;
			}
			//devono essere solo caratteri numerici
			if(comando[i]>57 || comando[i]<48){
				printf("Formato non valido!\n");
				return;
			}
			j++;
			index++;
			//se ho controllato il secondo valore per giorno e mese(k<2) o il quarto carattere per l'anno(k==3), devo verificare che ci siano i separatori adatti
			//la variabile j indica quale cifra di giorno/mese/anno stiamo controllando
			//la variabile k indica se stiamo controllando giorno(0)/mese(1)/anno(2)
			if((j==2 && k<2 ) || j==4){			
				startnum[index]='\0';
				num=atoi(startnum);
				j=0;
				//controllo della validità dei separatori
				switch(k){
					case 0:
						//mese e giorno sono separati da ':'
						if(num>31 || comando[i+1]!=':'){
							printf("Formato non valido!\n");
							return;
						}
						break;
					case 1:
						//mese e anno sono separati da ':'
						if(num>12 || comando[i+1]!=':'){
							printf("Formato non valido!\n");
							return;
						}
						break;
					case 2:
						//la prima e la seconda data sono separate da '-', la seconda data può essere seguita solo da spazi al massimo
						if((comando[i+1]!='-' && data_num==0) || ((comando[i+1]!=' ' && comando[i+1]!='\0')  && data_num==1)){
							printf("Formato non valido!\n");
								
							return;
						}
						break;
				}
				i++;
				k++;
				index=0;
			}
			i++;		
		}while(k<3);
		index=0;
		data_num++;
		k=0;	
	}
	date_1[10]=date_2[10]='\0';
	//controllo che le date siano giuste
	ret=controlla_data(date_1, date_2);
	if(ret<0){
		printf("Formato non valido!\n");
		return;
	}
	//dopo le date possono essere stati inseriti solo spazi 
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		printf("Formato non valido!\n");
		return;
	}
	get(aggr, type, date_1, date_2);
}