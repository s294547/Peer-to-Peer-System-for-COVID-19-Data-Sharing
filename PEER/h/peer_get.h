struct data_aggr* ask_aggr_to_neigh(int, int, int, int, int , int, char, char);
int ask_req_aggr(int, int, int, int, int, int, int, char, char);
struct data_aggr* calcola_totale(int, int, int, int, int, int, char);
struct data_aggr* calcola_variazione(int, int, int, int, int, int, char);
void controllo_get(char*);
void get(char, char, char*, char*);
struct data_aggr* get_neigh_aggr(int);
void get_updated_register(int, int, int, struct daily_register*);
void send_end_to_server(int);
void trova_dato(int, int, int, int, int, int, char, char);