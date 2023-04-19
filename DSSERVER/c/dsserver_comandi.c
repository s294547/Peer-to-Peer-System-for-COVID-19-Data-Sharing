#include "../h/dsserver_data.h"
#include "../h/dsserver_stop.h"


//funzione per il controllo degli argomenti inseriti dopo ./dsserver
int controllo_argomenti(int argc, char* argv[]){
	int len, i, port;
	//controllo che sia stato inserito un solo argomento
	if(argc>2 || argc==1)	
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

//rimuove tuttigli spazi inseriti in un comando
void rimuovi_spazi(char* comando){
	int i,j=0, len;
	char appoggio[32];
	//ricopio sul buffer di appoggio il comando, senza spazi
	for(i=0; comando[i]!='\0'; i++){
		if(comando[i]!=' '){
			appoggio[j]=comando[i];
			j++;
		}
	}
	appoggio[j]='\0';
	len=strlen(appoggio);
	//copio il buffer di appoggio, contenente il comando senza spazi, sulla stringa 'comando'
	strncpy(comando, appoggio, len);
	comando[len]='\0';
}

int find_port(char* comando){
	int i=0, port;
	//stringa di appoggio per il calcolo del numero di porta
	char* app;
	//dopo aver rimosso gli spazi dal comando, se il formato è corretto, il numero di porta dovrebbe iniziare a partire da comando[12]
	for(i=12; comando[i]!='\0'; i++){
		//se trovo un valore non numerico, il formato è non corretto
		if(comando[i]>57 || comando[i]<48){
			return -1;
		}
	}
	//ottengo il valore intero della porta
	app=comando+12;
	port=atoi(app);
	//controllo che il numero di porta sia valido
	if(port>65535)
		return -1;
	return port;
}

void help(){
	printf("showpeer\nMostra l’elenco dei peer connessi al network (IP e porta)\n\n");
	printf("showneighbor [peer]\nMostra i neighbor di un peer specificato come parametro opzionale. Se non c’è il parametro, il comando mostra i neighbor di ogni peer\n\n");
	printf("esc\nTermina il DS. La terminazione del DS causa la terminazione di tutti i peer\n\n");

}

void showpeer(){
	//puntatore che mi permette di scorrere la lista dei peer
	struct Peer* appoggio=lista_peer;
	int i=0;
	if(lista_peer==NULL){
		printf("Non ci sono peer registrati\n");
	}
	else{
		while(appoggio!=NULL){
			printf("********************************** PEER %d **********************************\n", i);
			printf("Indirizzo IP: 127.0.0.1\n");
			printf("Porta: %d\n", appoggio->port);
			appoggio=appoggio->next_peer;
			i++;
		}
	}
}


void showneighbor(int port){
 	//puntatore che mi permette di scorrere la lista dei peer
	struct Peer* appoggio=lista_peer;
	if(lista_peer==NULL){
		printf("Non ci sono peer registrati\n");
		return;
	} 
	//scorrimento della lista dei peer
	while(appoggio!=NULL){
			if(port==-1 || port==appoggio->port){
				printf("***************************** PEER CON PORTA %d *****************************\n", appoggio->port);
				if(appoggio->neighbor_1!=NULL)
					printf("<Neighbor 1> Indirizzo IP: 127.0.0.1 Porta:%d\n", appoggio->neighbor_1->port);
				if(appoggio->neighbor_2!=NULL)
					printf("<Neighbor 2> Indirizzo IP: 127.0.0.1 Porta:%d\n", appoggio->neighbor_2->port);
				if(appoggio->neighbor_1==NULL && appoggio->neighbor_2==NULL)
					printf("Il peer non ha vicini\n");
				if(port==appoggio->port)
					return;
			}
	appoggio=appoggio->next_peer;	
	}
	if(port!=-1){
		printf("Non esiste alcun peer con la porta specificata\n");
	}
} 

void esegui_comando(){
	int cmp, i=0, len, port;
	//stringa dove viene trasferito il comando inserito dall'utente
	char comando[32];
	//stringa di appoggio, utilizzata solo per verificare se il comando è showneighbor (l'unico con degli argomenti)
	char appoggio[32];
	//inserimento del comando
	fgets(comando, 31, stdin);
	rimuovi_spazi(comando);
	comando[strlen(comando)-1]='\0';
	appoggio[strlen(comando)]='\0';
	//ciclo per controllare se è stato inserito uno dei comandi validi
	for(i=0; i<4; i++){
		if(i!=2)
			cmp=strcmp(comandi[i], comando);
		//controllo se il comando sia showneighbor (comando[2]==showneighbor )
		else{
			len=strlen(comando);
			//trasferisco il nome del comando, senza argomenti(se il comando è stato inserito con formato corretto),su appoggio
			strncpy(appoggio, comando, 12);
			appoggio[12]='\0';
			//verifico che il contenuto di appoggio sia uguale a showneighbor (comando[2]==showneighbor )
			cmp=strcmp(comandi[i], appoggio);
			//caso in cui voglio vedere tutti i vicini, non è stato inserito il numero di porta
			if(len==12 && cmp==0){
				port=-1;
			}
			else{
				//ottengo il numero di porta, se è non corretto la funzione ritorna -1
				port=find_port(comando);
				//se il comando o il numero di porta sono in formato non corretto, non eseguo il comando
				if(len<12 || port<0)
					cmp=-1;
			}
			
			
		}
		if(cmp==0){
			switch(i){
				case 0:
					help();
					break;
				case 1:
					showpeer();
					break;
				case 2:
					showneighbor(port);
					break;
				case 3: 
					esc();
					break;
			}
			printf("Inserire un comando\n");	
			return;
		}
	}
	//se il comando inserito non corrisponde a nessuno dei comandi validi si richiede l'inserimento di un nuovo comando
	printf("Formato non valido! Inserire un comando\n");	
}