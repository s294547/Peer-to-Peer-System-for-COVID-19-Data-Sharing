//definisco una struttura Peer, mantiene il numero di porta, descrittore del socket tcp con cui scambiare messaggi con un peer connesso, i vicini e un puntatore al peer succesivo
struct Peer{
	int port;
	int tcp_sv_com; 
	struct Peer* neighbor_1;
	struct Peer* neighbor_2;
	struct Peer* next_peer;
};