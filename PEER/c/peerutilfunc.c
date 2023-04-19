#include "../h/peerdata.h"

int controllo_validita_giorni(int giorno, int mese){
	//controlla che la data inserita sia una data esistente
	if((giorno>28 && mese==2) || (giorno>30 && (mese==11 || mese==4 || mese==6 || mese==9 )))
		return -1;
	else
		return 0;
}

//controlla che le date inserite siano corrette
int controlla_data(char* data1, char* data2){
	time_t rawtime;
	struct tm* timeinfo;
	//controlla che data1 sia prima di data2, che data2 non sia la data corrente se siamo prima delle 18 e che le date siano esistenti
	int giorno1, giorno2, mese1, mese2, anno1, anno2, ret=0;
	//per prelevare le stringhe di giorni, mesi, anni da convertire poi numericamente
	char appoggio[11];
	if(data1[0]=='*' || data2[0]=='*')
		return ret;
	strncpy(appoggio, data1, 11);
	//inserisco i '\0' per ricavare le stringhe
	appoggio[2]=appoggio[5]='\0';
	giorno1=atoi(appoggio);
	mese1=atoi(appoggio+3);
	anno1=atoi(appoggio+6);
	strncpy(appoggio, data2, 11);
	appoggio[2]=appoggio[5]='\0';
	giorno2=atoi(appoggio);
	mese2=atoi(appoggio+3);
	anno2=atoi(appoggio+6);
	ret= controllo_validita_giorni(giorno1, mese1);
	if(ret<0)
		return ret;
	ret= controllo_validita_giorni(giorno2, mese2);
	//ottengo l'ora locale
	time(&rawtime);
	// Converto l'ora
	timeinfo = localtime(&rawtime);
	//se data 2 coincide alla data attuale e siamo prima delle 18
	if(timeinfo->tm_hour<18 && giorno2==timeinfo->tm_mday && mese2==timeinfo->tm_mon+1 && anno2==timeinfo->tm_year+1900){
		printf("Non è possibile utilizzare la data attuale, i registri non sono stati ancora chiusi\n");
		return -1;
	}

	if(ret<0)
		return ret;
	if(anno1>anno2 || (anno1==anno2 && mese1>mese2) || (anno1==anno2 && mese1==mese2 && giorno1>=giorno2)){
		ret=-1;
	 }
	return ret;
}

//funzione per la ricezione del messaggio contenente i nuovi vicini
void aggiorna_vicini(int peer_com){
	int ret, num_neigh;
	uint16_t msglen;
	char buff[1024];
	//ricezione della lunghezza del messaggio
	do{
		ret = recv(peer_com, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricezione del numero di vicini e dei numeri di porta dei vicini
	do{
		ret = recv(peer_com, buff, ntohs(msglen), 0);
		sscanf(buff,"%d %d %d", &num_neigh, &neigh[0], &neigh[1]);	
	}while(ret< ntohs(msglen));
	//notifico il server che ho ricevuto i vicini con il codice 'NGRCV'
	strncpy(buff, "NGRCV", 5);
	buff[5]='\0';
	do{
		ret = send(peer_com, buff, CODE_LEN, 0);		
	}while(ret<CODE_LEN);
	printf("Notifico al server l'avvenuta ricezione dei vicini, invio del codice 'NGRCV'\n");
	if(num_neigh==0)
		printf("Il client non ha vicini\n");
	else{
		printf("Numero di vicini:%d\n", num_neigh);
		if(neigh[0]!=-1)
			printf("Numero di porta del vicino %d:%d\n", 1,neigh[0]);			
		if(neigh[1]!=-1)
			printf("Numero di porta del secondo vicino %d:%d\n",2, neigh[1]);
	}
	return;
}

//funzione per l'invio di un dato aggregato richiesto da un vicino
void send_aggr(int peer_sock){
	struct data_aggr* appoggio;
	char buff[1024];
	uint16_t msglen;
	int day1, month1, year1, day2, month2, year2, ret;
	char type, aggrt, app=' ';
	//ricezione della lunghezza del messaggio con la richiesta del dato aggregato
	do{
		ret=recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t)); 
	//ricezione della richiesta del dato aggregato
	do{
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen)); 
	sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day1,&app, &month1,&app, &year1,&app, &day2,&app, &month2,&app, &year2, &aggrt,&ret, &type);
	printf("Ricevuta una richiesta del dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al %d/%d/%d\n", aggrt, type, day1, month1, year1, day2, month2, year2);
	//controllo se il dato aggregato è presente
	appoggio=find_aggr(day1, month1, year1, day2, month2, year2, aggrt, type);
	//se il dato aggregato non è presente, notifico il peer vicino
	if(appoggio==NULL){
		printf("Dato aggregato non presente\n");
		//invio un messaggio al vicino per dire che il dato non è presente
		strncpy(buff, "NAGGR", 5);
		buff[5]='\0';
		do{
			ret=send(peer_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio 'NAGGR' per notificare la mancanza del dato aggregato\n");
		}
		//se il dato aggregato è presente
	else{
		printf("Dato aggregato presente\n");
		//invio un messaggio al vicino per dire che il dato è presente
		strncpy(buff, "FAGGR", 5);
		buff[5]='\0';
		do{
			ret=send(peer_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio 'FAGGR' al vicino per notificare la presenza del dato aggregato\n");
		//creo il messaggio con il dato aggregato
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", appoggio->day1, app,  appoggio->month1, app,  appoggio->year1, app,  appoggio->day2, app,  appoggio->month2, app,  appoggio->year2, appoggio->aggrt, ret, appoggio->type, appoggio->data, app);
		//invio la lunghezza del messaggio con il dato aggregato
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio il dato aggregato
		do{
			ret=send(peer_sock, buff, strlen(buff)+1, 0);
		}while(ret<(strlen(buff)+1));	
		printf("Inviato il dato aggregato %c di tipo %c calcolato dal giorno %d/%d/%d al %d/%d/%d\n", aggrt, type, day1, month1, year1, day2, month2, year2);
	}
	//notifico il sever che un vicino ha inviato il dato aggregato 
	strncpy(buff,"FLEND", 5);
	buff[5]='\0';
	do{
		ret=send(server_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato il messaggio 'FLEND' al server\n");	
	close(peer_sock);
}

//funzione per ricevere i registri e i dati aggregati da parte di un peer vicino che sta uscendo o dal server, se non ci sono peer attualmente connessi al server
void ottieni_registri_aggr_vicino(int peer_sock, int sv){ //
	int ret, day, month, year, day2, month2, year2,  tot_tamponi, tot_casi, closed,  num_reg, num_aggr, data;
	struct daily_register* appoggio=NULL;
	struct data_aggr* appoggio2=NULL;
	uint16_t msglen;
	char buff[1024];
	char app, type, aggrt; 
	char str[30];
	if(sv){
		strcpy(str,"server");
	}
	else
		strcpy(str,"vicino che sta uscendo");

	//ricezione della lunghezza del messaggio con il numero di registri
	do{
		ret=recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricezione del numero dei registri
	do{
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff,"%d%c", &num_reg,&app);
	printf("Numero dei registri da ricevere:%d\n", num_reg);
	//ciclo per la ricezione dei registri del vicino/server
	while(num_reg>0){
		//ricezione della lunghezza del messaggio con il registro
		do{
			ret=recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//ricezione del registro
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
		sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &closed, &app);
		//se il registro è già presente, lo aggiorno solo se non è ancora stato chiuso e presenta un numero minore di casi o tamponi
		appoggio=find_daily_register(day, month, year);
		if(appoggio!=NULL){
			if(appoggio->closed==0 && (appoggio->tot_casi<tot_casi || appoggio->tot_tamponi<tot_tamponi)){
				appoggio->tot_casi=tot_casi;
				appoggio->tot_tamponi=tot_tamponi;
				appoggio->closed=closed;
			}
		}
		//se non è presente, ottengo il registro e lo salvo
		else{
			appoggio=create_register(day, month, year, tot_tamponi, tot_casi, closed);
		}
		num_reg--;
	}
	printf("Terminazione della ricezione dei registri\n");
	//ricezione della lunghezza del messaggio con il numero di dati aggregati
	do{
		ret=recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricezione del numero dei dati aggregati
	do{
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff,"%d%c", &num_aggr,&app);
	printf("Numero dei dati aggregati da ricevere:%d\n", num_aggr);
	//ciclo per la ricezione dei dati aggregati del vicino/server
	while(num_aggr>0){
		//ricezione della lunghezza del messaggio con il dato aggregato
		do{
			ret=recv(peer_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//ricezione del dato aggregato
		ret=recv(peer_sock, buff, ntohs(msglen), 0);
		sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &day2, &app, &month2, &app, &year2, &aggrt, &ret, &type,  &data,  &app);
		//se il dato aggregato è già presente, non devo fare nulla
		appoggio2=find_aggr(day, month, year, day2, month2, year2, aggrt, type);
		//se non è presente, ottengo il dato aggregato e lo salvo
		if(appoggio2==NULL){
			appoggio2=create_aggr(day, month, year, day2, month2, year2, aggrt, type, data);
		}
		num_aggr--;
	}
	
	//comunico al peer/server che ho ricevuto tutti i suoi registri e dati aggregati
	printf("Terminazione della ricezione dei dati aggregati\n");
	printf("Comunico al %s l'avvenuta ricezione dei registri e dei dati aggregti con 'SRACK'\n", str);
	strncpy(buff, "SRACK", 5);
	buff[5]='\0';
	do{
		ret=send(peer_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
}

//funzione per continuare o terminare il flooding di una entry del daily register
void start_flooding(int i, char type, int quantity, int day, int month, int year){
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
	
	//se non c'è il vicino o il flooding è terminato da un lato, segnalo al server che il flooding si è bloccato da un lato
	if(neigh[i]==-1 || (i==1 && peer_id>neigh[i]) || (i==0 && peer_id<neigh[i])){
		strncpy(buff,"FLEND", 5);
		buff[5]='\0';
		do{
			ret=send(server_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio 'FLEND' al server, non ci sono più vicini a cui inoltrare la nuova entry\n");	
	}
	//se c'è il vicino e non ho terminato il flooding, inoltro la richiesta di flooding
	else{
		peer_cl_sock=socket(AF_INET, SOCK_STREAM, 0);
		printf("Connessione al vicino di porta: %d per l'invio di una nuova entry del registro giornaliero\n", neigh[i]);
		do{
			ret=connect(peer_cl_sock, (struct sockaddr*)&neigh_addr, neigh_len);
		}
		while(ret<0);	
		//invio della richiesta di flooding per la nuova entry al vicino
		strncpy(buff,"FNEW", 4);
		//se inltro al vicino di sinistra, codice "FNEW0", se inoltro al vicino di destra codice "FNEW1"
		if(i==0)
			buff[4]='0';
		else
			buff[4]='1';
		buff[5]='\0';
		do{
			ret=send(peer_cl_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio '%s' al vicino di porta:%d\n",buff,neigh[i]);
		//creazione del messaggio con i dati da memorizzare e da inoltrare
		sprintf(buff, "%c%d%c%d%c%d%c%d%c", type, quantity, app, day, app, month, app, year, app);
		//invio la lunghezza del messaggio
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio al vicino i dati da memorizzare e da inoltrare
		do{
			ret=send(peer_cl_sock, buff, (strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		printf("Inviata una nuova entry del registro giornaliero (tipo:%c, quantità:%d) al vicino di porta:%d\n", type, quantity, neigh[i]);
		close(peer_cl_sock);
		printf("Chiusura del socket di comunicazione con il vicino di porta: %d\n", neigh[i]);
	}
}

//funzione per l'aggiornamento del daily register, aggiunge la nuova entry e aggiorna i dati totali del giorno corrente
void update_with_entry(struct daily_register* new_register, char type, int quantity){ //
	//aggiornamento dei dati totali del daily register
	if(type=='T')
		new_register->tot_tamponi+=quantity;
	else
		new_register->tot_casi+=quantity;
	printf("Aggiunta la entry di tipo:'%c' e quantità:%d al registro del giorno corrente.\n", type, quantity);
	printf( "%d/%d/%d) Tamponi effettuati=%d, Casi rilevati=%d\n", new_register->day, new_register->month, new_register->year, new_register->tot_tamponi, new_register->tot_casi);
}

//funzione per l'aggiornamento del daily register con la nuova entry e per continuare il flooding della nuova entry
void keep_flooding(int dir, char type, int quantity, int day, int month, int year){
	struct daily_register* appoggio=registers;
	struct daily_register* new_register;
	//controllo se è presente il daily register
	while(appoggio!=NULL){
		if(appoggio->day==day && (appoggio->month)==month && appoggio->year==year){
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
	//aggiorno il daily register con la nuova entry e modifico i dati totali
	update_with_entry(new_register, type, quantity);
	//continuo o termino il flooding del dato
	start_flooding(dir, type, quantity, day, month, year);
}

//funzione per inviare la risposta ad una richiesta di registro giornaliero fatta in flooding
void send_answer_flood(int i, int day, int month,int  year,int tot_tamponi, int tot_casi, int port_num){
	struct sockaddr_in neigh_addr;
	int neigh_len, ret;
	//socket usato per la connessione al vicino
	int peer_cl_sock;
	uint16_t msglen;
	char buff[1024], app=' ';
	//creazione dell'indirizzo del vicino
	memset(&neigh_addr, 0, sizeof(neigh_addr));
	neigh_len=sizeof(neigh_addr);
	neigh_addr.sin_family = AF_INET;
	inet_pton(AF_INET, peer_ip , &neigh_addr.sin_addr);
	neigh_addr.sin_port = htons(neigh[i]);
	peer_cl_sock=socket(AF_INET, SOCK_STREAM, 0);
	
	//è terminato il flooding, invio una risposta FANS0 o FANS1
	printf("Connessione al vicino di porta:%d per la rispondere alla richiesta del registro del giorno %d/%d/%d\n",neigh[i], day, month, year);
	do{
		ret=connect(peer_cl_sock, (struct sockaddr*)&neigh_addr, neigh_len);
	}
	while(ret<0);
	//invio messaggio "FANS0" o "FANS1" per segnalare il termine del flooding da un lato
	strncpy(buff,"FANS", 4);
	if(i==0)
		buff[4]='0';
	else
		buff[4]='1';
	buff[5]='\0';
	do{
		ret=send(peer_cl_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato il messaggio '%s' al vicino, è stato trovato il registro giornaliero richiesto dal peer %d\n", buff, port_num);
	// creo il messaggio con il registro corretto
	sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, port_num, app);
	//invio la lunghezza del messaggio con il registro
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio il registro
	do{
		ret=send(peer_cl_sock, buff, (strlen(buff)+1), 0);
	}while(ret<(strlen(buff)+1));
	printf("Inviato il messaggio con il registro del giorno %d/%d/%d richiesto dal peer %d al vicino\n", day, month, year, port_num);
	close(peer_cl_sock);
}

//funzione per il flooding della richiesta del registro di un giorno e per l'eventuale inizio della risposta
void req_register_flood(int i, int day, int month,int  year,int tot_tamponi, int tot_casi, int port_num){
	struct sockaddr_in neigh_addr;
	uint16_t msglen;
	int neigh_len, ret;
	//socket usato per la connessione al vicino
	int peer_cl_sock;
	char buff[1024], app=' ';
	//creazione dell'indirizzo del vicino
	memset(&neigh_addr, 0, sizeof(neigh_addr));
	neigh_len=sizeof(neigh_addr);
	neigh_addr.sin_family = AF_INET;
	inet_pton(AF_INET, peer_ip , &neigh_addr.sin_addr);
	neigh_addr.sin_port = htons(neigh[i]);
	peer_cl_sock=socket(AF_INET, SOCK_STREAM, 0);
	
	//se è terminato il flooding, invio una risposta FANS0 o FANS1
	if((i==1 && neigh[i]<peer_id) || (i==0 && neigh[i]>peer_id)){
		//devo inviare all'altro vicino, modifico il numero di porta
		neigh_addr.sin_port = htons(neigh[(i==0)?1:0]);
		printf("Connessione al vicino di porta:%d per la rispondere alla richiesta del registro del giorno %d/%d/%d da parte del peer %d\n",neigh[(i==0)?1:0], day, month, year, port_num);
		do{
			ret=connect(peer_cl_sock, (struct sockaddr*)&neigh_addr, neigh_len);
		}
		while(ret<0);
		//invio messaggio "FANS0" o "FANS1" per segnaalre il termine del flooding da un lato
		strncpy(buff,"FANS", 4);
		if(i==0)
			buff[4]='1';
		else
			buff[4]='0';
		buff[5]='\0';
		do{
			ret=send(peer_cl_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio '%s' al vicino, non ci sono più peer a cui inoltrare la richiesta\n", buff);
		//creo il messaggio con il registro così come è stato aggiornato fino ad ora
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, port_num, app);
		//invio la lunghezza del messaggio
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del registro così come è stato aggiornato fino ad ora
		do{
			ret=send(peer_cl_sock, buff, (strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		printf("Inviato il messaggio con il registro del giorno %d/%d/%d al vicino\n", day, month, year);
		close(peer_cl_sock);
	}
	//se c'è il vicino, non era presente il registro chiuso e non ho terminato il flooding, inoltro la richiesta di flooding
	else{
		printf("Connessione al vicino di porta:%d per inoltrare la richiesta del registro del giorno %d/%d/%d\n",neigh[i], day, month, year);
		do{
			ret=connect(peer_cl_sock, (struct sockaddr*)&neigh_addr, neigh_len);
		}
		while(ret<0);
		//inoltro della richiesta di flooding a sinistra(FREQ0) o a destra (FREQ1)
		strncpy(buff,"FREQ", 4);
		if(i==0)
			buff[4]='0';
		else
			buff[4]='1';
		buff[5]='\0';
		do{
			ret=send(peer_cl_sock, buff, CODE_LEN, 0);
		}while(ret<CODE_LEN);
		printf("Inviato il messaggio '%s' al vicino\n", buff);
		//creazione del messaggio con il registro con i dati aggiornati fino ad ora
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, port_num, app);
		//invio la lunghezza del messaggio
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del registro con i dati aggiornati fino ad ora
		do{
			ret=send(peer_cl_sock, buff, (strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		printf("Inviato il messaggio di richiesta da parte del peer %d con il registro del giorno fino ad ora più aggiornato %d/%d/%d al vicino\n",port_num, day, month, year);
		close(peer_cl_sock);
	}
}


//funzione che, una volta ricevuta la richiesta di un registro il flooding, controlla se è necessario proseguire il flooding o fornire una risposta
void check_register_req(int i, int peer_cl_sock ){
	struct daily_register* appoggio;
	int ret,day,month, year, tot_tamponi, tot_casi, port_num;
	uint16_t msglen;
	char buff[1024], app;
	//ricezione della lunghezza del messaggio con il registro del giorno richiesto
	do{
		ret=recv(peer_cl_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricezione del messaggio con il giorno del registro richiesto
	do{
		ret=recv(peer_cl_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &port_num, &app);
	printf("Ricevuta richiesta per il registro del giorno %d/%d/%d, parte del peer %d\n", day, month, year, port_num);
	//controllo se il registro del giorno è presente
	appoggio=find_daily_register(day, month, year);
	//se è presente ed è chiuso, termino il flooding da questo lato e invio una risposta 
	if( appoggio!=NULL && appoggio->closed==1){
		printf("Registro del giorno %d/%d/%d presente e chiuso, avvio della procedura di risposta al peer %d\n", day, month, year, port_num);
		send_answer_flood((i==0)?1:0, day,month, year, appoggio->tot_tamponi, appoggio->tot_casi, port_num);
	}
	//se non è presente o non è chiuso
	else{
		//se è presente ed è più aggiornato, aggiorno i dati e continuoa inviare la richeista, per vedere se ce n'è uno ancora più aggiornato
		if(appoggio!=NULL){
			printf("Registro del giorno %d/%d/%d presente, controllo se è più aggiornato\n", day, month, year);
			tot_tamponi=(appoggio->tot_tamponi>tot_tamponi)?appoggio->tot_tamponi:tot_tamponi;
			tot_casi=(appoggio->tot_casi>tot_casi)?appoggio->tot_casi:tot_casi;
		}
		else
			printf("Registro del giorno %d/%d/%d non presente\n", day, month, year);
		//invia la richiesta
		req_register_flood(i, day,month, year, tot_tamponi, tot_casi, port_num);
	}
}

//funzione per inviare il registro giornaliero richiesto da un peer vicino
void send_daily_register(int peer){
	struct daily_register* appoggio=registers;
	struct daily_register* new_register;
	int day, month, year, ret, neigh_port;
	uint16_t msglen;
	char app, buff[1024];
	//ricevo la lunghezza del messaggio con il registro richiesto
	do{
		ret=recv(peer, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricevo il giorno del registro richiesto dal vicino
	do{
		ret=recv(peer, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	sscanf(buff, "%c%d%c%d%c%d%c%d%c", &app, &day,&app,&month,&app,&year, &app, &neigh_port, &app);
	printf("Ricevuto il codice 'GETDR' dal vicino con numero di porta %d\n", neigh_port);
	printf("Ricevuto giorno del registro richiesto dal vicino con numero di porta %d\n", neigh_port);
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
	//creazione del messaggio con il daily register
	sprintf(buff, "%d %d", new_register->tot_tamponi, new_register->tot_casi);
	//invio del messaggio con la lunghezza del daily register
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(peer, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio del daily register
	do{
		ret=send(peer, buff, (strlen(buff)+1), 0);
	}while(ret<(strlen(buff)+1));
	printf("Inviato il daily register al vicino con numero di porta %d\n", neigh_port);
}
