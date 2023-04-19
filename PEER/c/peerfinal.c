#include "../h/peerdata.h"
#include "../h/peerutilfunc.h"
#include "../h/peer_start.h"
#include "../h/peer_add.h"
#include "../h/peer_get.h"
#include "../h/peer_stop.h"

int controllo_argomenti(int argc, char* argv[]){
	int len, i, port;
	//controllo che sia stato inserito un solo argomento
	if(argc>2 || argc==1)	
		if(!(argc==3 && strcmp("boot", argv[2])==0))
			return -1;
	len=strlen(argv[1]);
	//controllo che l'argomento sia costituito da soli numeri
	for(i=0; i<len; i++){
		if(argv[1][i]>57 || argv[1][i]<48){
			return -1;
		}
	}
	//controllo che l'argomento sia un numero di porta valido
	port=atoi(argv[1]);
	if(port>65535)
		return -1;
	return port;
}

int esegui_comando(){
	int cmp, i=0, ret=0;
	//stringa dove viene trasferito il comando inserito dall'utente
	char comando[64];
	//stringa di appoggio per verificare che comando è stato inserito, non è utile per verificare la validità degli argomenti
	char appoggio[64];
	//inserimento del comando da terminale
	fgets(comando, 63, stdin);
	comando[strlen(comando)-1]='\0';
	strncpy(appoggio, comando, 5);
	//i comandi hanno al più 5 lettere
	appoggio[5]='\0';
	//ciclo per controllare se è stato inserito uno dei comandi validi
	//se il comando inserito è valido, il controllo degli argomenti dei comandi è effettuato successivamente
	for(i=0; i<4; i++){
		//i comandi add e get (comando[1], comando[2]) sono costituiti da 3 lettere, anticipo la fine della stringa appoggio per farla coincidere con il comando
		if(i==1){
			appoggio[3]='\0';
		}
		//il comando stop (comando[3]) è costituito da 4 lettere
		if(i==3){
			appoggio[3]=comando[3];
			appoggio[4]='\0';
		}
			
		cmp=strcmp(comandi[i], appoggio);
		//controllo se il comando inserito da terminale è uno dei comandi validi
		//prima di mandare in esecuzione il comando, vengono effettuati i controlli di validità degli argomenti (tranne che per la stop())
		//i controlli di validità degli argomenti sono effettuati con funzioni specifiche (void controllo_nomecomando(char* comando_intero))
		//se i controlli hanno successo, alla fine viene eseguito il comando
		if(cmp==0){
			switch(i){
				case 0:
					if(start_called==0)
						controllo_start(comando);
					else 
						printf("Il peer è già connesso al server con IP:%s e numero di porta:%d\n", server_ip, server_port);
					break;
				case 1:
					if(start_called==1)
						controllo_add(comando);
					else 
						printf("Il peer non è connesso ad alcun server, non è possibile eseguire il comando richiesto\n");
					break;
				case 2:
					if(start_called==1)
						controllo_get(comando);
					else 
						printf("Il peer non è connesso ad alcun server, non è possibile eseguire il comando richiesto\n");
					break;
				case 3: 
					if(start_called==1){
						stop();
						ret=1;
					}
					else 
						printf("Il peer non è connesso ad alcun server, non è possibile eseguire il comando richiesto\n");
					break;
			}
			//dopo aver eseguito il comando, se ne richiede uno nuovo
			printf("Inserire un comando\n");	
			return ret;
		}
	}
	//se il comando inserito non corrisponde a nessuno dei comandi validi si richiede l'inserimento di un nuovo comando
	printf("Formato non valido! Inserire un comando\n");	
	return ret;
}


int main(int argc, char* argv[]){
	int ret, i, newfd, new_quantity, send_ret, dir, day, month, year, tot_tamponi, tot_casi, num_port, main_type=0;
	uint16_t msglen;
	char new_type, app=' ';
	char buff[1024]; // buffer per la ricezione dei messaggi
	fd_set read_fds; /* Set di lettura gestito dalla select */
	//struttura che conterrà i socket mandanti messaggi
	struct sockaddr_in sock_addr;
	int addrlen=sizeof(sock_addr);
	//controllo che sia stato inserito un numero di porta
	main_type=controllo_argomenti(argc, argv);
	if(main_type==-1){
		printf("Numero di porta non valido\n");
		return 0;
	}
	peer_id=main_type;
	peer_sock = socket(AF_INET, SOCK_DGRAM, 0);
	//creo l'indirizzo del socket del peer perchè è in ascolto sempre sulla solita porta
	memset(&peer_addr, 0, sizeof(peer_addr));
	peer_addr.sin_family = AF_INET;
	// INADDR_ANY mette il server in ascolto su tutte le
	// interfacce (indirizzi IP) disponibili sul server
	inet_pton(AF_INET, peer_ip , &peer_addr.sin_addr);
	peer_addr.sin_port = htons(peer_id);
	//faccio la bind del socket del peer
	ret=bind(peer_sock, (struct sockaddr*)&peer_addr, sizeof(peer_addr));
	//se non è possibile usare la porta, termino l'esecuzione del peer
	if(ret<0){
		perror("Error: ");
		if (errno == EADDRINUSE)
				printf("Non è stato possibile effettuare la bind perchè la porta scelta è già in utilizzo. Scegliere un numero di porta differente.\n");
		if(errno == EACCES)
				printf("Non è stato possibile effettuare la bind perchè non si hanno sufficienti privilegi per la porta scelta . Scegliere un numero di porta differente.\n");
		return 0;
	}

	printf("***************************** PEER COVID RUNNING *****************************\n");
	printf("Digita un comando:\n");
	printf("\n");
	printf("1) start DS_addr DS_port --> richiede al DS, in ascolto all’indirizzo DS_addr:DS_port, la connessione al network.");
	printf(" DS_addr deve avere formato: x.x.x.x con x esadecimale, DS_port deve avere formato numerico.\n");
	printf("2) add type quantity --> aggiunge al register della data corrente l’evento type con quantità quantity.");
	printf(" Type può assumere come valori 'T' o 'N', quantity è un valore numerico.\n");
	printf("3) get aggr type period --> effettua una richiesta di elaborazione per ottenere il dato aggregato aggr sui dati relativi a un lasso temporale d’interesse period sulle entry di tipo type.");
	printf(" Aggr può assumere come valore 'T' o 'V', type può assumere come valori 'T' o 'N', period deve essere costituito da due date in formato 'gg:mm:aaaa' separate da '-'. Se non si vuole specificare una delle due date, bisogna inserire'*'\n");
	printf("4) stop --> disconnessione dal network\n");
	printf("_\n");


	FD_ZERO(&master);
	FD_ZERO(&read_fds);
	//aggiungo stdin al set
	FD_SET(0, &master);
	//aggiungo il peer al socket
	FD_SET(peer_sock, &master);
	// Tengo traccia del maggiore 
	fdmax = peer_sock;
	//per effettuare il boot richiesto nelle specifiche del progetto
	if( argc==3 && strcmp("boot", argv[2])==0){
		start("127.0.0.1", 4242);	
	}
	for(;;){
		//se il socket TCP ottiene un nuovo descrittore, lo aggiungo al set
		if(!FD_ISSET(peer_sock, &read_fds)){
			FD_SET(peer_sock, &master);
			if(peer_sock > fdmax){ fdmax = peer_sock; }
		}
		read_fds = master; // read_fds sarà modificato dalla select
		select(fdmax + 1, &read_fds, NULL, NULL, NULL);
		for(i=0; i<=fdmax; i++) { // f1) Scorro il set
			if(FD_ISSET(i, &read_fds)) { // i1) Trovato desc. pronto
				// è stato inserito qualcosa da terminale
				if(i == peer_sock) { // se il peer riceve un messaggio
					// se è stata chiamata la funzione start
					if(start_called){
						addrlen = sizeof(sock_addr);
						newfd = accept(peer_sock, (struct sockaddr *)&sock_addr, (socklen_t*)&addrlen);
						printf("Accetto una richiesta di connessione\n");
						FD_SET(newfd, &master);
						if(newfd > fdmax){ fdmax = newfd; } // Aggiorno fdmax
						break;
					}
				}		
				else{
					if(i!=0){
						//ricevo il codice del messaggio
						ret = recv(i, buff, CODE_LEN, 0);
						if(ret==CODE_LEN && strcmp(buff,"NEIGH")==0){
							aggiorna_vicini(i);
							continue;
						}
						//richiesta di un server di fare la stop
						if(ret==CODE_LEN && strcmp(buff,"SVESC")==0){
							printf("Ricevuto messaggio 'SVESC' dal server, avvio la routine di terminazione\n");
							//salvataggio su file dei registri giornalieri
							stop();
							return 0;
						}
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
						//se il messaggio ricevuto è una richiesta di flooding
						if(ret==CODE_LEN && (strcmp(buff,"FNEW0")==0 || strcmp(buff,"FNEW1")==0)){
							printf("Ricevuto messaggio '%s' da un peer vicino\n", buff);
							if(strcmp(buff,"FNEW0")==0)		
								dir=0;
							else
								dir=1;
							//ricezione della lunghezza del messaggio con la entry del daily register
							do{
								send_ret=recv(i, (void*)&msglen, sizeof(uint16_t), 0);
							}while(send_ret<sizeof(uint16_t));
							//ricezione della entry del daily register
							do{
								send_ret=recv(i, buff, ntohs(msglen), 0);
							}while(send_ret<ntohs(msglen));
							sscanf(buff, "%c%d%c%d%c%d%c%d%c", &new_type, &new_quantity, &app, &day,&app,  &month, &app, &year, &app);
							//inoltro della nuova entry
							keep_flooding(dir, new_type, new_quantity, day, month, year);
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
						//ricevuta una risposta ad una richiesta di un registro giornaliero
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
						//chiusura del socket di comunicazione, a meno che non sia quello utilizzato per il server
						if(i!=server_sock){
							close(i);
							// Rimuovo il descrittore del socket dal set master
							FD_CLR(i, &master); 
						}
					}
				}	
				if(i==0){//se arriva un input per il peer
					ret=esegui_comando();
					//se è stata eseguita la stop
					if(ret==1){
						for(i=1; i<fdmax; i++)
							close(i);
						return 0;

					}
					continue;
				}
			}
		} // Fine for f1
	} // Fine for(;;)
}