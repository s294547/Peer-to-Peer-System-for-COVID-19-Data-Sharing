#include "../h/dsserver_data.h"

//funzione per la creazione del messaggio da mandare ad un peer con i suoi nuovi vicini
void create_message_neigh(char* buff, int num_neigh, int port){
	int port1, port2;
	struct Peer* appoggio=lista_peer;
	//ricerca del peer
	while(appoggio!=NULL){
		if(appoggio->port==port)
			break;
		else
			appoggio=appoggio->next_peer;
	}
	//se il viicno non è presente, si passa il numero di porta -1
	port1=(appoggio->neighbor_1!=NULL)?appoggio->neighbor_1->port:-1;
	port2=(appoggio->neighbor_2!=NULL)?appoggio->neighbor_2->port:-1;
	//creazione del messaggio
	sprintf(buff,"%d %d %d", num_neigh, port1, port2);
}

//funzione per la ricezione del codice STEND da parte di un peer, indica il termine dell'esecuzione della start
void receive_end_code(int port){
	int ret=0;
	char buff[1024];
	struct Peer* appoggio=lista_peer;
	//individuo il peer
	while(appoggio!=NULL){
		if(appoggio->port==port)
			break;
		else
			appoggio=appoggio->next_peer;
	}
	//attesa della ricezione di un codice di terminazione della start (STEND) da parte del peer
	do{
		ret = recv(appoggio->tcp_sv_com, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	if(strcmp(buff, "STEND")==0)
		return;
}

//funzione per la creazione e l'inserimento in lista di un peer con numero di porta peer_port
void set_peer(int peer_port){
	struct Peer* appoggio=lista_peer;
	struct Peer* peer_prec=NULL;
	struct Peer* new_peer=malloc(sizeof(struct Peer));
	new_peer->port=peer_port;
	new_peer->neighbor_1=NULL;
	new_peer->neighbor_2=NULL;
	
	//se è il primo, lo metto in testa
	if(lista_peer==NULL){
		new_peer->next_peer=lista_peer;
		lista_peer=new_peer;
		return;
	}
	//cerco la posizione giusta dove inserirlo, sono ordinati per numero di porta
	while(appoggio!=NULL){
		if(peer_port==appoggio->port)
			return;
		if(peer_port<appoggio->port){
			if(peer_prec==NULL){
				lista_peer=new_peer;
			}
			else{
				peer_prec->next_peer=new_peer;
			}
			new_peer->next_peer=appoggio;
			return;
		}
		peer_prec=appoggio;
		appoggio=appoggio->next_peer;	
	}
	//inserimento in coda
	peer_prec->next_peer=new_peer;
	new_peer->next_peer=NULL;
}


//funzione per l'aggiornamento dei vicini di un peer
void send_neighs(int port, int new_peer_port){
	int num_neigh=0, ret=-1, tcp_sv;
	uint16_t msglen;
	struct sockaddr_in peer_addr;
	char buff[1024];
	struct Peer* appoggio=lista_peer;
	//individuo il peer
	while(appoggio!=NULL){
		if(appoggio->port==port)
			break;
		else
			appoggio=appoggio->next_peer;
	}
	// se il peer è nuovo creo un nuovo socket per la comunicazione con quel peer, altrimenti recupero il socket per la comunicazione con quel peer
	if(port==new_peer_port){
			//socket TCP con comportamento da "client", invia i neigh al peer, che nel frattempo avrà creato il suo socket di ascolto tcp
			tcp_sv=socket(AF_INET, SOCK_STREAM, 0);
			appoggio->tcp_sv_com=tcp_sv;
			FD_SET(tcp_sv, &master);
			if(tcp_sv>fdmax)
				fdmax=tcp_sv;
	}
	else{
		tcp_sv=appoggio->tcp_sv_com;
	}
	//trovo il numero dei vicini
	num_neigh+=(appoggio->neighbor_1!=NULL);
	num_neigh+=(appoggio->neighbor_2!=NULL);

	// se il peer è nuovo, il socket tcp del server deve connettersi al socket del peer
	if(port==new_peer_port){
		while(1){
			//indirizzo del peer a cui mandare i neighbor
			memset(&peer_addr, 0, sizeof(peer_addr));
			peer_addr.sin_family=AF_INET;
			inet_pton(AF_INET, "127.0.0.1" , &peer_addr.sin_addr);
			peer_addr.sin_port = htons(port);
			ret=connect(tcp_sv,  (struct sockaddr*)&peer_addr, sizeof(peer_addr));
			if(ret>=0)
				break;
		}
	}
	//mando il codice NEIGH per segnalare l'inizio del trasferimento dei vicini ad un peer
	do{
		strncpy(buff, "NEIGH", 5);
		buff[5]='\0';
		ret = send(tcp_sv, buff, CODE_LEN, 0);
		//invio il messaggio finchè non è ricevuto correttamente
	}while (ret<CODE_LEN);
	//creo il messaggio da inviare
	create_message_neigh(buff,num_neigh, port);
	msglen=htons(strlen(buff)+1);
	//invio la lunghezza del messaggio
	do{
		ret = send(tcp_sv, (void*)&msglen, sizeof(uint16_t), 0);
		//invio il messaggio finchè non è ricevuto correttamente
	}while (ret<sizeof(uint16_t));
	//invio il messaggio finchè non è ricevuto correttamente
	do{
		ret = send(tcp_sv, buff, strlen(buff)+1, 0);
		//invio il messaggio finchè non è ricevuto correttamente
	}while (ret<strlen(buff)+1);
	//ricevo conferma che i vicini sono stati aggiornati
	do{	
		ret = recv(tcp_sv, buff, CODE_LEN, 0);		
	}while(ret<CODE_LEN || strcmp(buff, "NGRCV")!=0);
	//non chiudo il socket utilizzato perchè continuerà a servirmi
}

//funzione per inviare i registri e i dati aggregati ad un peer quando è l'unico ad essere connesso
void send_registers_aggr_to_peer(int comm_sock){
	int ret, num_reg,num_aggr, day, month, year,day2, month2, year2, tot_casi, tot_tamponi, closed, data;
	char buff[1024];
	char app=' ', type, aggrt;
	uint16_t msglen;
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
	file_saved=fopen(filename, "r");
	//se non ci sono file, metto come num_reg il numero 0
	if(file_saved==NULL)
		num_reg=0;
	else
		//ricavo il numero di registri da inviare
		fscanf(file_saved, "%d%c", &num_reg, &app);
	
	//creazione del messaggio con il numero di registri giornalieri
	sprintf(buff, "%d%c", num_reg, app);
	//invio la lunghezza del messaggio con il numero di registri giornalieri
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio il numero di registri giornalieri
	do{
		ret=send(comm_sock, buff, strlen(buff)+1 , 0);
	}while(ret<(strlen(buff)+1));

	//scorro la lista de registri giornalieri e invio ciascun registro al vicino/server
	while(num_reg>0){
		//ottengo il registro dal file
		fscanf(file_saved, "%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &tot_tamponi, &app, &tot_casi, &app, &closed, &app);
		//se il registro è di un giorno precedente al giorno corrente o è del giorno corrente ma sono state superate le sei, lo chiudo
		if((day<timeinfo->tm_mday && (month-1)==timeinfo->tm_mon && year==(timeinfo->tm_year+1900))|| ((month-1)<timeinfo->tm_mon && year==(timeinfo->tm_year+1900)) || (year<(timeinfo->tm_year+1900)) || ( timeinfo->tm_hour>=18 && day==timeinfo->tm_mday && (month-1)==timeinfo->tm_mon && year==(timeinfo->tm_year+1900)))
			closed=1;
		
		//creazione del messaggio con il registro
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, tot_tamponi, app, tot_casi, app, closed, app);
		//invio la lunghezza del messaggio con il registro
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del registro al peer	
		do{
			ret=send(comm_sock, buff,(strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		//prossimo registro
		num_reg--;
	}
	//chiusura del file dei registri
	if(file_saved!=NULL)
		fclose(file_saved);
	sprintf(filename, "%s%d", "svaggr", server_port);
	//apertura del file in lettura
	file_saved=fopen(filename, "r");
	//se non ci sono file, metto come num_agr il numero 0
	if(file_saved==NULL)
		num_aggr=0;
	else
		//ricavo il numero di dati aggregati da inviare
		fscanf(file_saved, "%d%c", &num_aggr, &app);
	//creazione del messaggio con il numero di dati aggregati
	sprintf(buff, "%d%c", num_aggr, app);
	//invio la lunghezza del messaggio con il numero di dati aggregati
	msglen=htons(strlen(buff)+1);
	do{
		ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
	}while(ret<sizeof(uint16_t));
	//invio il numero di dati aggregati
	do{
		ret=send(comm_sock, buff, strlen(buff)+1 , 0);
	}while(ret<(strlen(buff)+1));
	//scorro la lista dei dati aggregati e invio ciascun dato al vicino/server
	while(num_aggr>0){
		//ottengo il dato aggregato dal file
		fscanf(file_saved, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", &day, &app, &month, &app, &year, &app, &day2, &app, &month2, &app, &year2 ,&aggrt, &ret, &type, &data, &app);
		//creazione del messaggio con il dato aggregato
		sprintf(buff, "%d%c%d%c%d%c%d%c%d%c%d%c%d%c%d%c", day, app, month, app, year, app, day2, app, month2, app, year2 , aggrt, ret, type, data, app);	
		//invio la lunghezza del messaggio con il dato aggregato
		msglen=htons(strlen(buff)+1);
		do{
			ret=send(comm_sock, (void*)&msglen, sizeof(uint16_t), 0);
		}while(ret<sizeof(uint16_t));
		//invio del dato aggergato al peer	
		do{
			ret=send(comm_sock, buff,(strlen(buff)+1), 0);
		}while(ret<(strlen(buff)+1));
		//prossimo dato
		num_aggr--;
	}
	if(file_saved!=NULL)
		fclose(file_saved);
	//ricezione del codice 'SRACK' da parte del peer, che apprende di aver terminato la ricezione di registri giornalieri
	do{
		ret=recv(comm_sock, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN || strcmp(buff, "SRACK")!=0);
}

//funzione per l'aggiornamento dei vicini di un peer
void update_neigh(int port){
	struct Peer* peer_prec=NULL;
	struct Peer* appoggio=lista_peer;
	//trovo il peer
	while(appoggio!=NULL){
			if(appoggio->port==port)
				break;
			else{
				peer_prec=appoggio;
				appoggio=appoggio->next_peer;
			}
	}

	//se sono l'unico peer, non ho vicini
	if(lista_peer->next_peer==NULL){
		appoggio->neighbor_1=NULL;
		appoggio->neighbor_2=NULL;
		return;
	}
	//se ci sono solo due peer, manca uno dei vicini
	if(lista_peer->next_peer->next_peer==NULL){
		//se sono il primo dei due peer
		if(appoggio==lista_peer){
			appoggio->neighbor_1=NULL;
			appoggio->neighbor_2=appoggio->next_peer;
		}
		else{
			appoggio->neighbor_1=peer_prec;
			appoggio->neighbor_2=NULL;
		}
		return;	
	}
	//se ci sono più di due peer
	//se sono il primo, il primo vicino sarà l'ultimo della coda
	if(appoggio==lista_peer){
		peer_prec=lista_peer;
		while(peer_prec->next_peer!=NULL)
			peer_prec=peer_prec->next_peer;	
		appoggio->neighbor_1=peer_prec;
	}
	//se non sono il primo, il primo vicino sarà il precedente
	else
		appoggio->neighbor_1=peer_prec;
	//se sono l'ultimo, il secondo vicino sarà il primo peer
	if(appoggio->next_peer==NULL)
		appoggio->neighbor_2=lista_peer;
	//altrimenti è il successivo
	else
		appoggio->neighbor_2=appoggio->next_peer;
}

//funzione per l'aggiornamento dei vicini di un nuovo peer e per l'aggiornamento dei vicini dei suoi vicini 
void update_and_send_neighs(int port){
	struct Peer* appoggio=lista_peer;
	int ret;
	char buff[1024];
	//individuo il peer
	while(appoggio!=NULL){
			if(appoggio->port==port)
				break;
			else{
				appoggio=appoggio->next_peer;
			}
	}
	//aggioro i suoi vicini
	update_neigh(port);
	//invio al peeer i nuovi vicini
	send_neighs(port, port);
	//se c'è il vicino di 'sinistra'
	if(appoggio->neighbor_1!=NULL){
		//aggiorno i suoi vicini
		update_neigh(appoggio->neighbor_1->port);
		//gli invio i suoi nuovi vicini
		send_neighs(appoggio->neighbor_1->port, port);
	}

	//stessa cosa per il vicino di destra
	if(appoggio->neighbor_2!=NULL){
		update_neigh(appoggio->neighbor_2->port);
		send_neighs(appoggio->neighbor_2->port, port);
	}
	//notifico di aver aggiornato i vicini al peer
	strncpy(buff,"NGEND", 5);
	buff[5]='\0';
	do{
		ret = send(appoggio->tcp_sv_com, buff, CODE_LEN, 0);
	}while(ret<CODE_LEN);
	//se il peer è l'unico, invio i registri e i dati aggregati salvati
	if(lista_peer->next_peer==NULL)
		send_registers_aggr_to_peer(appoggio->tcp_sv_com);
	//il server attende la ricezione di un codice di terminazione della start (STEND) da parte del peer
	receive_end_code(port);
}