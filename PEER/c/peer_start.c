#include "../h/peerdata.h"
#include "../h/peerutilfunc.h"

//funzione per il caricamente dei registri di un peer durante la start
void carica_registri(){ //
	int i;
	char app;
	//recupero da file tutti i daily register del peer
	FILE* file_saved;
	int tot_registers, day, month, year, tot_tamponi, tot_casi, closed;
	//recupero dei nome del file (peer(numero_peer))
	char filename[10];
	sprintf(filename, "%s%d", "peer", peer_id);
	//apertura del file il lettura
	file_saved=fopen(filename, "r");
	if(file_saved==NULL){
		printf("Non sono presenti daily registers salvati per questo peer\n");
		return;
	}
	else
		printf("Apertura del file:%s\n", filename);	
	//recupero il numero di registri del file da leggere all'inizio del file stesso
	fscanf(file_saved, "%d%c", &tot_registers, &app);
	printf("Numero di registri da caricare:%d\n", tot_registers);
	//ciclo per la lettura dei registri  e per l'allocazione dei registri
	for(i=0; i<tot_registers; i++){
		fscanf(file_saved,"%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi,&app,  &closed, &app);
		//creazione del registro
		create_register(day, month, year, tot_tamponi, tot_casi, closed);
	}
	//chiusura del file
	if(file_saved!=NULL)
		fclose(file_saved);
	printf("Chiusura del file:%s\n", filename);
}

//funzione per l'ottenimento del daily register da parte di un vicino durante la 'start'
void get_daily_register(){
	int vicino=-1, comm_sock, ret, tot_tamponi, tot_casi, day, month, year;
	char buff[1024], app=' ';
	uint16_t msglen;
	struct daily_register* new_register;
	struct sockaddr_in neigh_addr;
	int addrlen=sizeof(neigh_addr);
	time_t rawtime;
	struct tm* timeinfo;
	//ottengo l'ora locale
	time(&rawtime);
	// Converto l'ora
	timeinfo = localtime(&rawtime);
	//inizializzo giorno, mese e anno con giorno, mese e anno correnti
	day=timeinfo->tm_mday, month=timeinfo->tm_mon+1, year=timeinfo->tm_year+1900;
	//se siamo oltre alle 18, il registro del giorno è quello del giorno successivo
	if(timeinfo->tm_hour>=18){
		day=get_next_day(day, month, year);
		month=get_next_month(day, month);
		year=get_next_year(day, month, year);
	}
	//se ha vicini, richiede il daily register a uno dei due
	if(neigh[0]!=-1)
		vicino=neigh[0];
	if(vicino==-1 && neigh[1]!=-1)
		vicino=neigh[1];
	if(vicino==-1)
		return;
	//creazione dell'indirizzo del socket tcp di uno dei vicini
	memset(&neigh_addr, 0, sizeof(neigh_addr));
	neigh_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1" , &neigh_addr.sin_addr);
	neigh_addr.sin_port = htons(vicino);
	//creazione del socket tcp per la comunicazione con il vicino
	printf("Creazione del socket TCP per la richiesta del registro giornaliero al vicino con porta:%d\n", vicino);
	comm_sock=socket(AF_INET, SOCK_STREAM, 0);
	//connessione al vicino
	ret=connect(comm_sock,(struct sockaddr*)&neigh_addr, addrlen);
	printf("Connesso al socket TCP del vicino con porta:%d\n", vicino);
	//invio del codice 'GETDR' per ottenere il daily register
	strncpy(buff, "GETDR", 5);
	buff[5]='\0';
	do{
		ret=send(comm_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Inviato il codice 'GETDR' al vicino con porta:%d\n", vicino);
	//creazione del messaggio con il giorno del registro che sarà richiesto al vicino (e il numero di porta)
	sprintf(buff, "%c%d%c%d%c%d%c%d%c",app, day,app,month,app,year, app, peer_id, app);
	//invio la lunghezza del messaggio
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio il giorno del registro che sarà richiesto al vicino (e il numero di porta)
	do{
		ret=send(comm_sock, buff, (strlen(buff)+1), 0);
	}while(ret<(strlen(buff)+1));
	printf("Inviato giorno del registro che verrà richiesto al vicino con porta:%d\n", vicino);
	//ricezione della lunghezza del messaggio con il daily register
	do{
		ret=recv(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//ricezione del daily register da parte del vicino
	do{
		ret=recv(comm_sock, buff, ntohs(msglen), 0);
	}while(ret<ntohs(msglen));
	printf("Ricevuto il daily register dal vicino con porta:%d\n", vicino);
	sscanf(buff, "%d %d", &tot_tamponi, &tot_casi);
	//ricerca del daily register, potrebbe possederne uno caricato da file
	new_register=find_daily_register(day, month, year);
	//creazione del daily register su struttura dati locale se non viene trovato
	if(new_register==NULL){
		printf("Creazione del registro giornaliero\n");
		new_register=create_daily_register(day, month, year);
	}
	//se il mio daily register è già chiuso, ho i dati aggiornati
	if(new_register->closed){
		printf("Il registro giornaliero a disposizione è già stato chiuso, non serve aggiornare i dati\n");
		//chiusura del socket di comunicazione
		printf("Chiusura del socket TCP per la comunicazione con il vicino con porta:%d\n", vicino);
		close(comm_sock);
		return;
	}
	//se il mio daily register ha dati minori rispetto a quello precedente, li aggiorno
	if(new_register->tot_tamponi<tot_tamponi || new_register->tot_casi< tot_casi){
		//aggiornamento del daily register con i dati ricevuti dal vicino
		printf("Aggiornamento dei dati del daily register\n");
		new_register->tot_tamponi=tot_tamponi;
		new_register->tot_casi=tot_casi;
		printf( "Tamponi effettuati=%d, Casi rilevati=%d\n", new_register->tot_tamponi, new_register->tot_casi);
	}
	//chiusura del socket di comunicazione col peer vicino
	printf("Chiusura del socket TCP per la comunicazione con il vicino con porta:%d\n", vicino);
	close(comm_sock);
}

//funzione per il caricamente dei dati aggregati di un peer durante la start
void carica_aggr(){
	int i;
	char app, type, aggrt;
	//recupero da file tutti i dati aggregati del peer
	FILE* file_saved;
	int day1, month1, year1, day2, month2, year2, data, tot_aggr, ret;
	//recupero dei nome del file (aggr(numero_peer))
	char filename[10];
	sprintf(filename, "%s%d", "aggr", peer_id);
	//apertura del file il lettura
	file_saved=fopen(filename, "r");
	if(file_saved==NULL){
		printf("Non sono presenti dati aggregati salvati per questo peer\n");
		return;
	}
	else
		printf("Apertura del file:%s\n", filename);	
	//recupero il numero di dati aggregati del file da leggere all'inizio del file stesso
	fscanf(file_saved, "%d%c", &tot_aggr, &app);
	printf("Numero di dati aggregati da caricare:%d\n", tot_aggr);
	//ciclo per la lettura dei dati aggregati da file file e per l'allocazione dei dtai aggregati
	for(i=0; i<tot_aggr; i++){
		fscanf(file_saved,"%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day1, &app, &month1, &app, &year1,&app, &day2, &app, &month2, &app, &year2, &aggrt,&ret, &type, &data, &app);
		//creazione del dato aggregato e inserimento nella lista dei dati aggregati
		create_aggr(day1, month1, year1, day2, month2, year2, aggrt, type, data);
	}
	//chiusura del file
	if(file_saved!=NULL)
		fclose(file_saved);
	printf("Chiusura del file:%s\n", filename);
}

//funzione per la connessione di un peer al dsserver e per la sua integrazione ai peer già connessi al dsserver
void start(char* ip, int port){ 
	int ret=0, peer_com;
	fd_set read_fd; /* Set di lettura, peer che ha ricevuto la risposta dal server */
	struct timeval timeout; // timeout per l'attesa della risposta dal server
	struct sockaddr_in sv_addr, tcp_sv_addr; // Indirizzo server
	int sv_len, tcp_sv_len;
	char buff[1024];
	//salvataggio dell'indirizzo ip e del numero di porta del server
	//strncpy(server_ip, ip, strlen(ip));
	server_port=port;
	printf("Richiedo la connessione al network di indirizzo IP:%s e porta:%d\n", ip, port);
	//
	memset(&sv_addr, 0, sizeof(sv_addr));
	sv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, ip , &sv_addr.sin_addr);
	sv_addr.sin_port = htons(port);
	FD_ZERO(&read_fd);
	// Aggiungo il peer al set
	FD_SET(peer_sock, &master);
	// Tengo traccia del maggiore, che è l'unico elemento del set
	fdmax = peer_sock;
	//invio del messaggio di start al server
	strncpy(buff, "START", 5);
	buff[5]='\0';
	ret=sendto(peer_sock, buff, 6, 0, (struct sockaddr*)&sv_addr, (socklen_t)sizeof(sv_addr));
	printf("Inviato messaggio di START al server di indirizzo IP:%s e porta:%d\n", ip, port);
	for(;;){
		//imposto il timeout di 5 secondi, se dopo 5 secondi il server non mi risponde invio nuovamente il messaggio di start
		timeout.tv_sec=5;
		timeout.tv_usec=0;
		read_fd = master; // read_fds sarà modificato dalla select
		select(fdmax + 1, &read_fd, NULL, NULL, &timeout);
		if(FD_ISSET(peer_sock, &read_fd)) { // i1) Trovato desc. pronto, il server ha inviato un messaggio
			// se il messaggio non è stato inviato correttamente, lo rimando
			if(ret<6){
				printf("Il messaggio di START non è stato inviato correttamente al server di indirizzo IP:%s e porta:%d\n", ip, port);
				strncpy(buff, "START", 5);
				ret=sendto(peer_sock, buff, 6, 0, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
				continue;
			}
			//ricevo l'ACK e controllo che sia un ACK
			ret=recvfrom(peer_sock, buff, 4, 0, (struct sockaddr*)&sv_addr, (socklen_t*)&sv_len);
			if(strcmp(buff, "ACK")==0){
				printf("Il messaggio di ACK inviato dal server di indirizzo IP:%s e porta:%d è stato correttamente ricevuto\n", ip, port);
				break;
			}	
			else{
				printf("Il messaggio di ACK inviato dal server di indirizzo IP:%s e porta:%d non è stato correttamente ricevuto, ritrasmissione del messaggio di START\n", ip, port);
				strncpy(buff, "START", 5);
				buff[5]='\0';
				ret=sendto(peer_sock, buff, 6, 0, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
			}
		}
		//se non ho ricevuto l'ack dopo 5 secondi rimado il messaggio di start
		else{
			printf("Il server di indirizzo IP:%s e porta:%d non ha mandato alcun messaggio di ACK, ritrasmissione del messaggio di START\n", ip, port);
			strncpy(buff, "START", 5);
			buff[5]='\0';
			ret=sendto(peer_sock, buff, 6, 0, (struct sockaddr*)&sv_addr, sizeof(sv_addr));
		}
	}	
	//rimuovo il socket udp dal set globale
	FD_CLR(peer_sock, &master);
	//chiusura del socket udp, non servirà più
	do{
		ret=close(peer_sock);
	}while( ret<0);
	printf("Chiusura del socket UDP\n");
	//creo un nuovo socket sulla stessa porta, questa volta TCP
	printf("Creazione del socket TCP per accettare richieste di connessione da altri peer o dal server\n");
	do{
		peer_sock = socket(AF_INET, SOCK_STREAM, 0);
	}while(peer_sock<0);
	//aggiunta del nuovo peer sock al set globale
	FD_SET(peer_sock, &master);
	//aggiornamento del valore fdmax
	if(peer_sock>fdmax)
		fdmax=peer_sock;
	tcp_sv_len=sizeof(tcp_sv_addr);
	//il socket creato, una volta chiuso, non mantiente l'indirizzo di porta inutilizzabile per 100-120 s con questa opzione
	if (setsockopt(peer_sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0)
   		printf("setsockopt(SO_REUSEADDR) failed\n");
	//faccio la bind del socket del peer
	do{
		ret=bind(peer_sock, (struct sockaddr*)&peer_addr, (socklen_t)sizeof(peer_addr));
		if(ret>=0)
			printf("Bind del socket TCP\n");
		else
			perror("Errore bind:");
	}while(ret<0);
	do{
		ret=listen(peer_sock, 50);
	}while(ret<0);
	printf("Il socket TCP si mette in ascolto di richieste di connessione\n");
	peer_com=accept(peer_sock, (struct sockaddr*)&tcp_sv_addr, (socklen_t*)&tcp_sv_len);
	//mantengo il socket di comunicazione con il server
	server_sock=peer_com;
	//aggiungo il socket di comunicazione con il server al set globale
	FD_SET(server_sock, &master);
	if(server_sock > fdmax){ fdmax = server_sock; } // Aggiorno fdmax
	printf("Il socket TCP ha accettato una richiesta di connessione dal parte del server per ricevere i suoi vicini\n");
	//ricezione del codice NEIGH
	do{
		ret = recv(peer_com, buff, CODE_LEN, 0);
		printf("Ricevuto codice: %s\n", buff);
	}while(ret<CODE_LEN); 
	//ricezione del messaggio con i nuovi vicini
	aggiorna_vicini(peer_com);
	do{
		ret = recv(peer_com, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN && strcmp(buff, "NGEND")!=0);
	//caricamento dei registri giornalieri salvati se il peer era stato chiuso
	carica_registri();
	//caricamento dei dati aggregati salvati se il peer era stato chiuso
	carica_aggr();
	//se ho dei vicini, mi faccio mandare il daily register
	get_daily_register();	
	//se non ho vicini, il server mi manda gli ultimi registri salvati
	if(neigh[0]==-1 && neigh[1]==-1)
		ottieni_registri_aggr_vicino(peer_com, 1);
	//il peer ha terminato esecuzione della start, invia quindi un codice di terminazione 'STEND' al server
	strncpy(buff,"STEND", 5);
	buff[5]='\0';
	do{
		ret = send(peer_com, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	printf("Invio del codice di terminazione 'STEND' al server\n");	
	//setto la variabile start_called in modo da non poter più chiamare la funzione start
	start_called=1;
	return;
}

void controllo_start(char* comando){
	//funzione per il controllo del formato degli argomenti del comando 'start' sia corretto
	int i=0, k=0, index=0, num, port, ipcont=0;
	char startnum[64];
	char ip[16];
	ip[15]='\0';
	//dopo il comando, deve esserci almeno uno spazio
	if(comando[5]!=' '){
		printf("Formato non valido!\n");
		return;
	}
	//controllo che dopo il comando vengano inseriti solo spazi o un valore numerico
	for(i=5; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		//appena viene trovato un valore numerico, si passa alla verifica della validità del formato dell'indirizzo Ip
		if(comando[i]<=57 && comando[i]>=48){
			break;
		}
		printf("Formato non valido!\n");
		return;
	}
	//ciclo per il controllo della validità dell'indirizzo Ip
	//la variabile k tiene conto di quale dei 4 esadecimali dell'indirizzo Ip stiamo controllando
	do{
		//ciclo per verificare il formato di ciascuno dei 4 esadecimali dell'indirizzo Ip
		while(comando[i]!='.' && !(k==3 && comando[i]==' ')){
			//su startnum trasferisco la stringa contente uno dei 4 esadecimali
			startnum[index]=comando[i];
			//su ip trasferisco la stringa contente l'intero indirizzo Ip, che do in ingresso alla funzione start()
			ip[ipcont]=comando[i];
			//controllo che siano stati inseriti solo valori numerici
			if(comando[i]>57 || comando[i]<48){
				printf("Formato dell'indirizzo Ip non valido!\n");
				return;
			}
			i++;
			index++;	
			ipcont++;
		}
		startnum[index]='\0';
		num=atoi(startnum);
		//controllo che gli esadecimali non superino il valore 255
		if(num>255){
			printf("Formato dell'indirizzo Ip non valido!\n");
			return;
		}
		index=0;
		ip[ipcont]=comando[i];
		ipcont++;
		k++;
		i++;
	}while(k<4);
	ip[ipcont]='\0';
	i--;
	//controllo che l'indirizzo ip e la porta siano separati da uno spazio
	if(comando[i]!=' '){
		printf("Formato non valido!\n");
		return;
	}	
	//controllo che vi siano solo spazi o al più un valore numerico, corrispondente al primo numero della porta
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		if(comando[i]<=57 && comando[i]>=48){
			break;
		}
		printf("Formato non valido!\n");
		return;
	}
	//controlo che il numero di porta sia valido
	while(comando[i]!=' ' && i<strlen(comando)){

		//startnum conterrà la stringa con il numero di porta
		startnum[index]=comando[i];
		//il numero di porta deve avere solo caratteri numerici
		if(comando[i]>57 || comando[i]<48){
			printf("Formato del numero di porta non valido!\n");
			return;
		}
		i++;		
		index++;	
	}
	startnum[index]='\0';
	//converto il numero di porta a intero
	port=atoi(startnum);
	if(port>65535){
		printf("Formato del numero di porta non valido!\n");
		return;
	}
	//dopo il numero di porta possono esseere stati inseriti solo spazi
	for(; i<strlen(comando) ;i++){
		if(comando[i]==' ')
			continue;
		printf("Formato non valido!\n");
		return;
	}
	start(ip,port);
}