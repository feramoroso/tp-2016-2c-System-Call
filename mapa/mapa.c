/***************************************************************************************************************************************************/
/************************************************         PROCESO MAPA         *********************************************************************/
/***************************************************************************************************************************************************/

/* STANDARD */
#include <dirent.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* SOCKETS */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

/* HILOS */
#include <pthread.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

/* PKMN BATTLE */
#include <pkmn/battle.h>
#include <pkmn/factory.h>

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

/***************************************************************************************************************************************************/
/************************************************         DEFINICIONES         *********************************************************************/
/***************************************************************************************************************************************************/
#define MAX_CON     100    // Conexiones máximas
#define T_NOM_HOST  128    // Tamaño del nombre del Server
#define TAM_MENSAJE 256    // Tamaño del mensaje Cliente/Servidor
#define RUTA_LOG "mapa.log"

/* Estructura para METADATA MAPA */
typedef struct {
	char     *nombre;
	char     *medalla;    // Ruta del archivo de la medalla del Mapa
	uint32_t  pokeNestCant;
	uint32_t  tiempoDeadlock;
	uint32_t  batalla;
	char     *algoritmo;
	uint32_t  quantum;
	uint32_t  retardo;
	char     *ip;
	uint32_t  puerto;
} tMapaMetadata;

/* Estructura para METADATA POKENEST */
typedef struct {
	char     *nombre;
	char     *tipo;
	char      id;
	uint32_t  posx;
	uint32_t  posy;
	t_queue  *pokemons;
} tPokeNestMetadata;

/* Estructura para METADATA POKEMON */
typedef struct {
	char       id;
	uint32_t   ord;
	t_pokemon *data;
} tPokemonMetadata;

/* Estructura para ENTRENADOR */
typedef struct {
	int       socket;
	char      id;
	char      obj;        // Proximo objetivo dentro del Mapa
	time_t    time;
	uint32_t  posx;
	uint32_t  posy;
	t_list   *pokemons;   // Lista de Pokemons ordenada por nivel
} tEntrenador;

/***************************************************************************************************************************************************/
/************************************************      VARIABLES GLOBALES      *********************************************************************/
/***************************************************************************************************************************************************/
tMapaMetadata     *mapaMetadata;        // Estructura con la Metadaata del Mapa
tPokeNestMetadata *pokeNestArray[100];  // Arreglo con las distintas PokeNest
tPokemonMetadata  *pokemonMetadata;     // Estructura con la Metadaata de los Pokemon

t_list  *eReady;                        // Lista de Entrenadores Listos
t_list  *eBlocked;                      // Lista de Entrenadores Bloqueados
t_list  *eDeadlock;                     // Lista de Entrenadores en Deadlock
t_list  *items;                         // Lista de elementos a graficar

char *rutaPokeDex;                      // Ruta del PokeDex

pthread_mutex_t mutexBlocked = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexReady   = PTHREAD_MUTEX_INITIALIZER;

sem_t semAsignador;                     // Semaforo PRODUCTOR/CONSUMIDOR para nuevos elementos Bloqueados
sem_t semPlanificador;                  // Semaforo PRODUCTOR/CONSUMIDOR para nuevos elementos Ready

t_log *logger;                          // Log

char matPedidos      [100][100];        // Matriz de Recursos Solicitados por los entrenadores bloqueados
char matAsignados    [100][100];        // Matriz de Recursos Asignados a los entrenadores bloqueados
char vecDisponibles  [100];             // Vector de Recursos Disponibles

/***************************************************************************************************************************************************/
/************************************************         FUNCIONES            *********************************************************************/
/***************************************************************************************************************************************************/
void getMapaMetadata() {
	t_config *mapConfig = config_create(string_from_format("%s/Mapas/%s/metadata", rutaPokeDex, mapaMetadata->nombre));
	if (mapConfig == NULL) {
		puts("No se encontro el mapa.");
		exit(EXIT_FAILURE);
	}
	mapaMetadata->tiempoDeadlock  = config_get_int_value(mapConfig, "TiempoChequeoDeadlock");
	mapaMetadata->batalla         = config_get_int_value(mapConfig, "Batalla");
	mapaMetadata->algoritmo       = strdup( config_get_string_value(mapConfig, "algoritmo") );
	mapaMetadata->quantum         = config_get_int_value(mapConfig, "quantum");
	mapaMetadata->retardo         = config_get_int_value(mapConfig, "retardo");
	mapaMetadata->ip              = strdup( config_get_string_value(mapConfig, "IP") );
	mapaMetadata->puerto          = config_get_int_value(mapConfig, "Puerto");
	config_destroy(mapConfig);
}
void imprimirInfoMapa() {
	printf("\nNombre del Mapa:                 %s", mapaMetadata->nombre);
	printf("\nRuta de la Medalla               %s", mapaMetadata->medalla);
	printf("\nTiempo de chequeo DeadLock (ms): %d", mapaMetadata->tiempoDeadlock);
	printf("\nBatalla:                         %d", mapaMetadata->batalla);
	printf("\nAlgoritmo:                       %s", mapaMetadata->algoritmo);
	printf("\nQuantum:                         %d", mapaMetadata->quantum);
	printf("\nRetardo entre Quantums (ms):     %d", mapaMetadata->retardo);
	printf("\nIP Mapa:                         %s", mapaMetadata->ip);
	printf("\nPuerto:                          %d\n\n",mapaMetadata->puerto);
}
/* Obtiene la posicion de la PokeNest id en el pokeNestArray */
int getPokeNestFromID(char id) {
	int i = 0;
	while (pokeNestArray[i]) {
		if (pokeNestArray[i]->id == id)
			return i;
		i++;
	}
	return -1;
}
void devolverPokemons(tEntrenador *entrenador) {
	tPokemonMetadata *pokemon;
	while(!list_is_empty(entrenador->pokemons)) {
		pokemon = list_remove(entrenador->pokemons, 0);
		int i = 0;
		while(pokeNestArray[i]->id != pokemon->id)
			i++;
		if(pokeNestArray[i] != NULL) {
			queue_push(pokeNestArray[i]->pokemons, pokemon);
			sumarRecurso(items, pokeNestArray[i]->id);
		}
	}
}
void desconectarEntrenador(tEntrenador *entrenador) {
	devolverPokemons(entrenador);
	BorrarItem(items, entrenador->id);
	log_info(logger, "Entrenador %c Desconectado!", entrenador->id);
	printf("Entrenador %c Desconectado!                                           ", entrenador->id);
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
	close(entrenador->socket);
	free(entrenador);
}
int pokeNestArraySize() {
	int i = 0;
	while (pokeNestArray[i])
	i++;
	return i;
}
tPokeNestMetadata *getPokeNestMetadata(char * nomPokeNest) {
	tPokeNestMetadata *pokeNestMetadata = malloc(sizeof(tPokeNestMetadata));
	pokeNestMetadata->nombre = strdup( nomPokeNest );
	t_config *pokeNestConfig = config_create(string_from_format("%s/Mapas/%s/PokeNests/%s/metadata", rutaPokeDex, mapaMetadata->nombre, nomPokeNest));
	if (pokeNestConfig == NULL) return NULL; //Chequeo Errores
	pokeNestMetadata->tipo = strdup( config_get_string_value(pokeNestConfig, "Tipo") );
	pokeNestMetadata->posx = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[0]);
	pokeNestMetadata->posy = atoi(string_split(config_get_string_value(pokeNestConfig, "Posicion"), ";")[1]);
	pokeNestMetadata->id   = config_get_string_value(pokeNestConfig, "Identificador")[0];
	config_destroy(pokeNestConfig);
	return pokeNestMetadata;
}
tPokemonMetadata *getPokemonMetadata(char *nomPokeNest, char id, int ord, char *rutaPokeNest) {
	t_pkmn_factory* pokemon_factory = create_pkmn_factory();
	tPokemonMetadata *pokemonMetadata = malloc(sizeof(tPokemonMetadata));
	t_config *pokemonConfig = config_create(string_from_format("%s/%s%03d.dat", rutaPokeNest, nomPokeNest, ord));
	if (pokemonConfig == NULL) return NULL; //Chequeo Errores
	pokemonMetadata->data = create_pokemon(pokemon_factory, nomPokeNest, config_get_int_value(pokemonConfig, "Nivel"));
	pokemonMetadata->id   = id;
	pokemonMetadata->ord  = ord;
	config_destroy(pokemonConfig);
	destroy_pkmn_factory(pokemon_factory);
	return pokemonMetadata;
}
int getPokeNestArray() {
	int i = 0;
	DIR *dir;
	struct dirent *dirPokeNest;
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/PokeNests/", rutaPokeDex, mapaMetadata->nombre);
	dir = opendir(ruta);
	while ((dirPokeNest = readdir(dir))) {
		if ( (dirPokeNest->d_type == DT_DIR) && (strcmp(dirPokeNest->d_name, ".")) && (strcmp(dirPokeNest->d_name, "..")) ) {
			if ( (pokeNestArray[i] = getPokeNestMetadata(dirPokeNest->d_name)) == NULL) {
				closedir(dir);
				return EXIT_FAILURE;
			}
			i++;
		}
	}
	pokeNestArray[i] = NULL;
	closedir(dir);
	return EXIT_SUCCESS;
}
int getPokemonsQueue() {
	int i = 0, j;
	tPokemonMetadata *pokemonMetadata;
	while (pokeNestArray[i]) {
		pokeNestArray[i]->pokemons = queue_create();
		char ruta[256];
		sprintf(ruta, "%s/Mapas/%s/PokeNests/%s", rutaPokeDex, mapaMetadata->nombre, pokeNestArray[i]->nombre);
		j = 1;
		while ((pokemonMetadata = getPokemonMetadata(pokeNestArray[i]->nombre, pokeNestArray[i]->id, j, ruta))) {
			queue_push(pokeNestArray[i]->pokemons, pokemonMetadata);
			j++;
		}
		i++;
	}
	return EXIT_SUCCESS;
}
void imprimirInfoPokeNest() {
	int i = 0;
	while(pokeNestArray[i]) {
		printf("\nPokeNest:         %s", pokeNestArray[i]->nombre);
		printf("\nTipo:             %s", pokeNestArray[i]->tipo);
		printf("\nPosición en x:    %d", pokeNestArray[i]->posx);
		printf("\nPosición en y:    %d", pokeNestArray[i]->posy);
		printf("\nIdentificador:    %c\n", pokeNestArray[i]->id);
		if(!queue_is_empty(pokeNestArray[i]->pokemons)) {
			printf("\nInstancias:        %d", queue_size(pokeNestArray[i]->pokemons));
			printf("\nNombre 1º en cola: %s", (*(tPokemonMetadata*)queue_peek(pokeNestArray[i]->pokemons)).data->species);
			printf("\nTipo 1º en cola:   %s", pkmn_type_to_string((*(tPokemonMetadata*)queue_peek(pokeNestArray[i]->pokemons)).data->type));
			printf("\nTipo 1º en cola:   %s", pkmn_type_to_string((*(tPokemonMetadata*)queue_peek(pokeNestArray[i]->pokemons)).data->second_type));
			printf("\nNivel 1º en cola:  %d\n\n\n", (*(tPokemonMetadata*)queue_peek(pokeNestArray[i]->pokemons)).data->level);
		}
		i++;
	}
}
/************************************************    FUNCIONES PLANIFICADOR    *********************************************************************/
void agregarReady(tEntrenador *entrenador) {
	char lista[256], *aux;
	pthread_mutex_lock(&mutexReady);
	list_add(eReady, entrenador);
	log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
	strcpy(lista, "Lista Ready:");
	int i = 0;
	while ( i < list_size(eReady) ) {
		aux = string_from_format(" %c", ((tEntrenador*)list_get(eReady, i))->id);
		strcat(lista, aux);
		free(aux);
		i++;
	}
	log_info(logger, lista);
	pthread_mutex_unlock(&mutexReady);
	sem_post(&semPlanificador);
}
void agregarBlocked(tEntrenador *entrenador) {
	char lista[TAM_MENSAJE], *aux;
	pthread_mutex_lock(&mutexBlocked);
	list_add(eBlocked, entrenador);
	log_info(logger, "Entrenador %c agregado a la cola de Bloqueados.", entrenador->id);
	strcpy(lista, "Lista Bloqueados:");
	int i = 0;
	while ( i < list_size(eBlocked) ) {
		aux = string_from_format(" %c", ((tEntrenador*)list_get(eBlocked, i))->id);
		strcat(lista, aux);
		free(aux);
		i++;
	}
	log_info(logger, lista);
	pthread_mutex_unlock(&mutexBlocked);
	sem_post(&semAsignador);
}
int distanciaObjetivo(tEntrenador *entrenador) {
	int x, y, i = 0;
	if (pokeNestArray[i] && entrenador->obj) {
		while(pokeNestArray[i]->id != entrenador->obj)
			i++;
		if(pokeNestArray[i] != NULL) {
			x = abs(pokeNestArray[i]->posx - entrenador->posx);
			y = abs(pokeNestArray[i]->posy - entrenador->posy);
			return x + y;
		}
		else
			return -2; // Objetivo no encontrado o PokeNest Vacia
	}
	return -1; // Objetivo no establecido o PokeNest Vacia
}
void moverEntrenador(tEntrenador *entrenador, char eje) {
	switch (eje) {
	case 'R':
		entrenador->posx++;
		break;
	case 'L':
		entrenador->posx--;
		break;
	case 'D':
		entrenador->posy++;
		break;
	case 'U':
		entrenador->posy--;
		break;
	}
	MoverPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
	send(entrenador->socket, "OK", 2, 0);
}
int enviarCoordenadasEntrenador(tEntrenador *entrenador, char pokeNest) {
	int pos = getPokeNestFromID(pokeNest);
	if ( pos == -1 ) {
		send(entrenador->socket, "N", 1, 0);
		desconectarEntrenador(entrenador);
		return EXIT_FAILURE;
	}
	entrenador->obj = pokeNest;
	send(entrenador->socket, string_from_format("%3d%3d", pokeNestArray[pos]->posx, pokeNestArray[pos]->posy), 6, 0);
	printf("Entrenador %c en busqueda de Pokemon %s.                    ", entrenador->id, pokeNestArray[pos]->nombre);
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
	return EXIT_SUCCESS;
}
void solicitarPokemon(tEntrenador *entrenador, char pokeNest) {
	int pos;
	pos = getPokeNestFromID(pokeNest);
	agregarBlocked(entrenador);
	printf("Entrenador %c ha solicitado Pokemon %s.                     ", entrenador->id, pokeNestArray[pos]->nombre);
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
}
void enviarMedalla(tEntrenador *entrenador) {
	send(entrenador->socket, mapaMetadata->medalla, strlen(mapaMetadata->medalla), 0);
	log_info(logger, "Entrenador %c ha completado su objetivo.", entrenador->id);
	desconectarEntrenador(entrenador);
}
/************************************************    FUNCIONES ORDENADORAS     *********************************************************************/
bool _menor_distancia(tEntrenador *entrenador, tEntrenador *entrenador_menor_distancia) {
    return distanciaObjetivo(entrenador) < distanciaObjetivo(entrenador_menor_distancia);
}
bool _mayor_tiempo(tEntrenador *entrenador, tEntrenador *entrenador_mayor_tiempo) {
    return entrenador->time > entrenador_mayor_tiempo->time;
}
bool _mayor_nivel(tPokemonMetadata *pokemon, tPokemonMetadata *pokemon_mayor_nivel) {
    return pokemon->data->level > pokemon_mayor_nivel->data->level;
}
/************************************************     FUNCIONES DEADLOCK       *********************************************************************/
void inicializarEstructuras(int eMax, int pMax) {
	int i, j;
	for(i = 0; i < eMax; i++) {
		for(j = 0; j < pMax; j++) {
			matPedidos[i][j]   = 0;
			matAsignados[i][j] = 0;
		}
		vecDisponibles[i] = 0;
	}
}
void llenarEstructuras(int eMax, int pMax) {
	int i, j, k;
	tEntrenador *entrenador;
	tPokemonMetadata *pokemon;
	for(i = 0; i < eMax; i++) {
		entrenador = list_get(eBlocked, i);
		matPedidos[i][getPokeNestFromID(entrenador->obj)] = 1;
		for(j = 0; j < list_size(entrenador->pokemons); j++) {
			pokemon = list_get(entrenador->pokemons, j);
			k = 0;
			while(pokeNestArray[k]->id != pokemon->id)
				k++;
			matAsignados[i][k]++;
		}
		for(k = 0; k < pMax; k++)
			vecDisponibles[k] = queue_size(pokeNestArray[k]->pokemons);
	}
}
void imprimirEstructuras(int eMax, int pMax) {
    FILE *tablas;
    tablas = fopen(RUTA_LOG, "a");
	int i,j;
    /***************************** ENCABEZADO ******************************************/
	fprintf(tablas, "\nEntrenador   -   Asignados    -    Pedidos    -    Disponibles");
    fprintf(tablas, "\n                 ");
    for(j = 0; j < pMax; j++)
		fprintf(tablas, "%c ", pokeNestArray[j]->id);
    fprintf(tablas, "          ");
    for(j = 0; j < pMax; j++)
    		fprintf(tablas, "%c ", pokeNestArray[j]->id);
    fprintf(tablas, "        ");
    for(j = 0; j < pMax; j++)
    		fprintf(tablas, "%c ", pokeNestArray[j]->id);
	/***********************************************************************************/

	for(i = 0; i < eMax; i++) {
		fprintf(tablas, "\n    %c            ", ((tEntrenador*)list_get(eBlocked, i))->id);
		for(j = 0; j < pMax; j++)
			fprintf(tablas, "%d ", matAsignados[i][j]);
		fprintf(tablas, "          ");
		for(j = 0; j < pMax; j++)
			fprintf(tablas, "%d ", matPedidos[i][j]);
		if(i == 0) {
			fprintf(tablas, "        ");
			for(j = 0; j < pMax; j++)
				fprintf(tablas, "%d ", vecDisponibles[j]);
		}
	}
	fprintf(tablas, "\n\n");
	fclose(tablas);
}
int deadlockDetect(int eMax, int pMax) {
	tEntrenador *entrenador;
	int i, j, again, flag = 0,
	work[pMax],                 /* Copio el vector de Recursos Disponibles */
	finish[eMax];               /* Vector con todos los procesos, 1 finaliza, 0 no finaliza */

	/* Inicializo vectores work y finish */
	for (i = 0; i < pMax; i++)
		work[i] = vecDisponibles[i];
	/* Asumo que terminan salvo que tengan recursos asignados */
	for (i = 0; i < eMax; i++) {
		finish[i] = 1;
		for (j = 0; j < pMax; j++)
			if ( matAsignados[i][j] ) {
				finish[i] = 0;
				break;
			}
	}
	do {
		again = 0;
		for (i = 0; i < eMax; i++) {
			if ( !finish[i] ) {
				for (j = 0; j < pMax; j++)
					if ( matPedidos[i][j] > work[j] ) {
						flag = 1;
						break;
					}
				if ( !flag ) {
					for (j = 0; j < pMax; j++)
						work[j] += matAsignados[i][j];
					finish[i] = 1;
					again = 1;
				}
			}
		}
	} while (again);

	/* Si algun proceso esta en 0, se encuentra en Deadlock */
	flag = 0;
	for (i = 0; i < eMax; i++) {
		if ( !finish[i] ) {
			/* Al encontrar el primero ya se que hay un Deadlock */
			flag = 1;
			/* Si esta activado el modo batalla agrego a los entrenadores en la lista Deadlock */
			if ( mapaMetadata->batalla ) {
				entrenador = list_get(eBlocked, i);
				list_add(eDeadlock, entrenador);
				log_info(logger, "Entrenador %c agregado a la cola de Deadlock.", entrenador->id);
				/* Por protocolo le aviso al entrenador que esta en Deadlock enviandole una "D" */
				send(entrenador->socket, "D", 1, 0);
				usleep(1000);
			}
		}
	}
	if ( flag ) {
		log_warning(logger, "Interbloqueo Detectado!");
		imprimirEstructuras(eMax, pMax);
		return 1;
	}
	return 0;
}
int deadlockDetect1(int eMax, int pMax) {
	tEntrenador *entrenador;
	int i, j, nuevo, flag = 0,
	work[pMax],                 /* Copio el vector de Recursos Disponibles */
	finish[eMax];               /* Vector con todos los procesos, 1 finaliza, 0 no finaliza */

	/* Inicializo vectores work y finish */
	for (i = 0; i < pMax; i++)
		work[i] = vecDisponibles[i];
	/* Asumo que terminan salvo que tengan recursos asignados */
	for (i = 0; i < eMax; i++) {
		finish[i] = 1;
		for (j = 0; j < pMax; j++)
			if ( matAsignados[i][j] ) {
				finish[i] = 0;
				break;
			}
	}

	/* Búsqueda de ciclos que impliquen interbloqueo */
	nuevo = 0;
	do {
		for (i = 0; i < eMax; i++) {
			if ( !finish[i] ) {
				/* Guardo en j el recurso que esta pidiendo */
				for (j = 0; j < pMax; j++)
					if ( matPedidos[i][j] )
						break;
				/*******************************************/
				if ( !work[j] )
					continue;
				finish[i] = 1;
				for (j = 0; j < pMax; j++)
					work[j] += matAsignados[i][j];
				nuevo = 1;
				break;
			}
		}
	} while ( nuevo );

	/* Si algun proceso esta en 0, se encuentra en Deadlock */
	for (i = 0; i < eMax; i++) {
		if ( !finish[i] ) {
			/* Al encontrar el primero ya se que hay un Deadlock */
			flag = 1;
			/* Si esta activado el modo batalla agrego a los entrenadores en la lista Deadlock */
			if ( mapaMetadata->batalla ) {
				entrenador = list_get(eBlocked, i);
				list_add(eDeadlock, entrenador);
				log_info(logger, "Entrenador %c agregado a la cola de Deadlock.", entrenador->id);
				/* Por protocolo le aviso al entrenador que esta en Deadlock enviandole una "D" */
				send(entrenador->socket, "D", 1, 0);
				usleep(1000);
			}
		}
	}
	if ( flag ) {
		log_warning(logger, "Interbloqueo Detectado!");
		imprimirEstructuras(eMax, pMax);
		return 1;
	}
	return 0;
}
int deadlockDetect2(int eMax, int pMax) {
	tEntrenador *entrenador;
	int m[100], vecTemp[100],
		found = 0, flag = 0,
		i, j, k, l, suma;

	k = 0;
	for(i = 0; i < eMax; i++) {
		suma = 0;
		for(j = 0; j < pMax; j++)
			suma += matAsignados[i][j];
		if(suma == 0) {
			m[k] = i;
			k++;
		}
	}

	for(i = 0; i < eMax; i++) {
		for(l = 0; l < k; l++)
			if(i != m[l]) {
				flag = 1;
				for(j = 0; j < pMax; j++)
					if(matPedidos[i][j] < vecTemp[j]) {
						flag = 0;
						break;
					}
			}
		if( flag ) {
			m[k] = i;
			k++;
			for(j = 0; j < pMax; j++)
				vecTemp[j] += matAsignados[i][j];
		}
	}
	/* Procesos en Deadlock */
	for(i = 0; i < eMax; i++) {
		found = 1;
		for(j = 0; j < k; j++)
			if(i == m[j])
				found = 0;
		if( found ) {
			entrenador = list_get(eBlocked, i);
			/* Si esta activado el modo batalla agrego a los entrenadores en la lista Deadlock */
			if ( mapaMetadata->batalla ) {
				list_add(eDeadlock, entrenador);
				log_info(logger, "Entrenador %c agregado a la cola de Deadlock.", entrenador->id);
			}
		}
	}
	/* Notifico a los entrenadores en Deadlock */
	if(list_size(eDeadlock) > 1) {
		i = 0;
		while(list_size(eDeadlock) > i) {
			/* Le aviso al entrenador que esta en Deadlock */
			entrenador = list_get(eDeadlock, i);
			send(entrenador->socket, "D", 1, 0);
			usleep(10000);
			i++;
		}
		/* Si se detecta Deadlock imprimo tablas y logueo */
		log_warning(logger, "Interbloqueo Detectado!");
		imprimirEstructuras(eMax, pMax);
		return 1;
	}
	list_clean(eDeadlock);
	return 0;
}
void batallaPokemon() {
	tEntrenador *eA, *eB, *eWinner, *eLoser;
	tPokemonMetadata *pkA, *pkB, *pkLoser;
	int i;
	pkLoser = malloc(sizeof(tPokemonMetadata));
	list_sort(eDeadlock, (void*)_mayor_tiempo);                     // Ordeno la lista por mayor tiempo
	while( list_size(eDeadlock) > 1 ) {

		eA = list_remove(eDeadlock, 0);                             // Saco el primer entrenador de la Lista de Deadlock
		list_sort(eA->pokemons, (void*)_mayor_nivel);               // Ordeno los pokemons por mayor nivel
		pkA = list_get(eA->pokemons, 0);                            // Tomo al pokemon de mayor nivel

		eB = list_remove(eDeadlock, 0);                             // Saco el segundo entrenador de la Lista de Deadlock
		list_sort(eB->pokemons, (void*)_mayor_nivel);               // Ordeno los pokemons por mayor nivel
		pkB = list_get(eB->pokemons, 0);                            // Tomo al pokemon de mayor nivel

	    log_info(logger, "****  Batalla Pokemon!  -  %c  Vs.  %c  ****", eA->id, eB->id);
	    log_info(logger, "%s nivel: %d  Vs.  %s nivel: %d", pkA->data->species, pkA->data->level, pkB->data->species, pkB->data->level);
	    pkLoser->data = pkmn_battle(pkA->data, pkB->data);          // Batalla!
	    log_info(logger, "%s nivel: %d fue derrotado!", pkLoser->data->species, pkLoser->data->level);
		if(pkLoser->data == pkB->data) {
			eWinner = eA;
			eLoser  = eB;
		}
		else {
			eWinner = eB;
			eLoser  = eA;
		}
		/* Al entrenador perdedor lo vuelvo a ingresar a la lista de Deadlock en la primera posición para la proxima batalla */
		list_add_in_index(eDeadlock, 0, eLoser);
    	/* Le aviso a el entrenador ganador que zafo */
    	send(eWinner->socket, "R", 1, 0);
	    log_info(logger, "****    Ganador  %c  -  Perdedor  %c    ****", eWinner->id, eLoser->id);

	    /* Si solo queda el perderdor lo mato, lo remuevo de la lista de bloqueados y lo desconecto */
	    if ( list_size(eDeadlock) == 1) {
	    	eLoser = list_remove(eDeadlock, 0);
	    	i = 0;
	    	while(eLoser != (tEntrenador*)list_get(eBlocked, i))
	    		i++;
	    	/* Le aviso a el entrenador que perdio */
	    	send(eLoser->socket, "K", 1, 0);
	    	log_info(logger, "Entrenador %c ha perdido la batala Pokemon!", eLoser->id);
	    	list_remove(eBlocked, i);
	    	desconectarEntrenador(eLoser);
	    }
	}
	free(pkLoser);
}
/************************************************        HILO DEADLOCK         *********************************************************************/
void *deadlock() {
	int eMax, pMax;
	pMax = pokeNestArraySize();
	while(1) {
		usleep( mapaMetadata->tiempoDeadlock * 1000 );
		pthread_mutex_lock(&mutexBlocked);
		if ( (eMax = list_size(eBlocked)) > 1 ) {
			inicializarEstructuras(eMax, pMax);
			llenarEstructuras(eMax, pMax);
			if ( deadlockDetect(eMax, pMax) && mapaMetadata->batalla )
				batallaPokemon();
		}
		pthread_mutex_unlock(&mutexBlocked);
	}
}
/************************************************  HILO I/0 ASIGNACION DE PKS  *********************************************************************/
void *asignador() {
	tEntrenador *entrenador;
	tPokemonMetadata *pokemon;
	int pos;
	char mensaje[TAM_MENSAJE];
	while (1) {
		usleep( mapaMetadata->retardo * 1000 );
		sem_wait(&semAsignador);
		pthread_mutex_lock(&mutexBlocked);
		if (!list_is_empty(eBlocked)) {
			entrenador = list_remove(eBlocked, 0);
			pos = getPokeNestFromID(entrenador->obj);
			if(!queue_is_empty(pokeNestArray[pos]->pokemons)) {
				pokemon = queue_pop(pokeNestArray[pos]->pokemons);
				list_add(entrenador->pokemons, pokemon);
				restarRecurso(items, pokemon->id);
				/* Envio ruta del Pokemon */
				sprintf(mensaje,"/Mapas/%s/PokeNests/%s/%s%03d.dat", mapaMetadata->nombre, pokeNestArray[pos]->nombre, pokeNestArray[pos]->nombre, pokemon->ord);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);
				log_info(logger, "Pokemon %s capturado por entrenador %c.", pokeNestArray[pos]->nombre, entrenador->id);
				printf("Pokemon %s capturado por entrenador %c!                     ", pokeNestArray[pos]->nombre, entrenador->id);
				fflush(stdout);
				nivel_gui_dibujar(items, mapaMetadata->nombre);
				entrenador->obj = 0;
				agregarReady(entrenador);
			}
			else {
				list_add(eBlocked, entrenador);
				sem_post(&semAsignador);
			}
		}
		pthread_mutex_unlock(&mutexBlocked);
	}
}
/************************************************   PLANIFICADOR ROUND ROBIN   *********************************************************************/
void planRR() {
	int ql = mapaMetadata->quantum;          // Quantum restante
	char mensajeCliente[TAM_MENSAJE];
	pthread_mutex_lock(&mutexReady);
	tEntrenador *entrenador = list_remove(eReady, 0);
    pthread_mutex_unlock(&mutexReady);
	log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (ql) {
		ql--;
		usleep( mapaMetadata->retardo * 1000 );
	    if (recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0) < 1) {
			desconectarEntrenador(entrenador);
	        return;
	    }
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			if( enviarCoordenadasEntrenador(entrenador, mensajeCliente[1]) )
				return;
			break;
		/* MOVER AL ENTRENADOR */
		case 'M':
			moverEntrenador(entrenador, mensajeCliente[1]);
			break;
		/* CAPTURAR EL POKEMON */
		case 'G':
			solicitarPokemon(entrenador, mensajeCliente[1]);
			return;
		/* OBTENER MEDALLA */
		case 'O':
			enviarMedalla(entrenador);
			return;
		}
	}
	agregarReady(entrenador);
}
/************************************************      PLANIFICADOR SRDF       *********************************************************************/
void planSRDF() {
    char mensajeCliente[TAM_MENSAJE];
    tEntrenador *entrenador;
    pthread_mutex_lock(&mutexReady);
    int disMenor = 999, dis, idx = 0,  i = 0;
    while( i < list_size(eReady)) {
    	entrenador = list_get(eReady, i);
    	dis = distanciaObjetivo(entrenador);
    	if (dis < disMenor) {
    		disMenor = dis;
    		idx = i;
    	}
    	i++;
    }
    entrenador = list_remove(eReady, idx);   // Saco al de menor distancia
    pthread_mutex_unlock(&mutexReady);
    log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (1) {
		usleep( mapaMetadata->retardo * 1000 );
	    if (recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0) < 1) {
			desconectarEntrenador(entrenador);
	        return;
	    }
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			if( enviarCoordenadasEntrenador(entrenador, mensajeCliente[1]) )
				return;
			agregarReady(entrenador);
			return;
		/* MOVER AL ENTRENADOR */
		case 'M':
			moverEntrenador(entrenador, mensajeCliente[1]);
			break;
		/* CAPTURAR EL POKEMON */
		case 'G':
			solicitarPokemon(entrenador, mensajeCliente[1]);
			return;
		/* OBTENER MEDALLA */
		case 'O':
			enviarMedalla(entrenador);
			return;
		}
	}
}
/************************************************      HILO PLANIFICADOR       *********************************************************************/
void *planificador() {
	while (1) {
		sem_wait(&semPlanificador);
//		if (!list_is_empty(eReady)) {
		    if      ( !strncmp(mapaMetadata->algoritmo, "RR"  , 2) )
				planRR();
			else if ( !strncmp(mapaMetadata->algoritmo, "SRDF", 4) )
				planSRDF();
//		}
	}
}
/************************************************        HILO HANDSHAKE        *********************************************************************/
void *handshake(void *socket) {
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];
	printf("Nueva Conexión!                                             ");
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
    tEntrenador *entrenador = malloc(sizeof(tEntrenador));
    entrenador->socket   = *(int*)socket;   //Castea el descriptor del socket
    entrenador->time     = time(0);
    entrenador->posx     = 1;
    entrenador->posy     = 1;
    entrenador->pokemons = list_create();
    entrenador->obj      = 0;
    /* Envio la bienvenida */
    sprintf(mensajeServer, "***************  Bienvenido a %s  ***************", mapaMetadata->nombre);
    send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
    /* Recibo el simbolo del entrenador */
    if (recv(entrenador->socket, mensajeCliente, 1, 0) < 1) {
		desconectarEntrenador(entrenador);
        return EXIT_SUCCESS;
    }
    entrenador->id = mensajeCliente[0];
    CrearPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
    log_info(logger, "Entrenador %c Conectado!", entrenador->id);
    printf("Bienvenido Entrenador %c                                    ", entrenador->id);
    fflush(stdout);
    nivel_gui_dibujar(items, mapaMetadata->nombre);
	agregarReady(entrenador);
	return EXIT_SUCCESS;
}
/***************************************************************************************************************************************************/
/************************************************             MAIN             *********************************************************************/
/***************************************************************************************************************************************************/
int main(int argc , char *argv[]) {
	system("clear");
	system("setterm -cursor off");
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: mapa <nombre_mapa> <ruta PokeDex>");
		return EXIT_FAILURE;
	}
/**************************************************   SECCION METADATA   ***************************************************************************/
	/* OBTENER METADATA DEL MAPA */
	mapaMetadata         = malloc(sizeof(tMapaMetadata));
	mapaMetadata->nombre = argv[1];
	rutaPokeDex          = realpath(argv[2], NULL);
	getMapaMetadata();

	/* OBTENER LA RUTA DE LA MEDALLA */
	mapaMetadata->medalla = string_from_format("/Mapas/%s/medalla-%s.jpg", mapaMetadata->nombre, mapaMetadata->nombre);

	imprimirInfoMapa(mapaMetadata);

	/* OBTENER ARREGLO DE POKENEST */
	if (getPokeNestArray(pokeNestArray, mapaMetadata->nombre, rutaPokeDex)) {
		puts("PokeNest invalida.");
		return EXIT_FAILURE;
	}
	/* OBTENER COLA DE POKEMONS EN CADA POKENEST */
	if (getPokemonsQueue(pokeNestArray, mapaMetadata->nombre, rutaPokeDex)) {
		puts("Error en la PokeNest.\n");
		return EXIT_FAILURE;
	}

	imprimirInfoPokeNest(pokeNestArray);

	/* LISTAS DE ENTRENADORES */
	eReady    = list_create();  // Lista de Entrenadores Listos para Ejecucíon
	eBlocked  = list_create();  // Lista de Entrenadores Bloqueados para Captura de Pokemons
	eDeadlock = list_create();  // Lista de Entrenadores en Interbloqueo

	pthread_mutex_init(&mutexBlocked, NULL);
	pthread_mutex_init(&mutexReady,   NULL);
	sem_init(&semAsignador   , 0, 0);
	sem_init(&semPlanificador, 0, 0);

	/* LOG */
	logger = log_create(RUTA_LOG, mapaMetadata->nombre, false, LOG_LEVEL_INFO);

/**************************************************   SECCION SEÑALES   ****************************************************************************/
	signal(SIGUSR2, getMapaMetadata);

/**************************************************   SECCION SOCKETS   ****************************************************************************/
	int socketEscucha, socketCliente;
	int addrlen;                                       // Tamaño de Bytes aceptados por accept
    struct sockaddr_in server, cliente;
	char hostname[T_NOM_HOST];                         // Cadena para almacenar el nombre del host del Server

	/* Creacion del Socket TCP */
    socketEscucha = socket(AF_INET, SOCK_STREAM, 0);
    if (socketEscucha == -1) {
        perror("socket");
        return EXIT_FAILURE;
    }

    /* Preparando la estructura */
	server.sin_family = AF_INET;
	server.sin_port = htons(mapaMetadata->puerto);   // Puerto extraido del archivo metadata
	inet_aton(mapaMetadata->ip, &(server.sin_addr)); // IP extraida del archivo metadata
	memset(&(server.sin_zero), '\0', 8);             // Pongo en 0 el resto de la estructura

	/* Mostrar información de Conexión */
	gethostname(hostname, T_NOM_HOST);
	printf("Host Name : %s\n", hostname);
	printf("Dirección : %s\n", inet_ntoa(server.sin_addr));
	printf("Puerto    : %d\n\n", mapaMetadata->puerto);

	/* Enlace */
    if( bind(socketEscucha, (struct sockaddr *)&server , sizeof(server)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }
    puts("Bind exitoso!");

    /* Pongo a escuchar el Socket */
    if( listen(socketEscucha, MAX_CON) == -1) {
    	perror("listen");
    	return EXIT_FAILURE;
    }
    puts("Escuchando conexiones...");

/**************************************************   SECCION GRAFICO   ****************************************************************************/
    puts("Presione Enter para iniciar...");
    getchar();
    items = list_create();  // Creo la Lista de elementos del Mapa
    /* Creo las diferentes PokeNest recorriendo el arreglo de PokeNest*/
    int i = 0;
    while (pokeNestArray[i]) {
    	CrearCaja(items, pokeNestArray[i]->id, pokeNestArray[i]->posx, pokeNestArray[i]->posy, queue_size(pokeNestArray[i]->pokemons));
    	i++;
    }
    nivel_gui_inicializar();
    nivel_gui_dibujar(items, mapaMetadata->nombre);
    printf("Esperando entrenadores...");
	fflush(stdout);
    nivel_gui_dibujar(items, mapaMetadata->nombre);

/**************************************************   SECCION PLANIFICACION   **********************************************************************/
	pthread_t hiloPlanificador;
	pthread_create( &hiloPlanificador, NULL, planificador, NULL);

/**************************************************    SECCION ASIGNACION     **********************************************************************/
	pthread_t hiloAsignador;
	pthread_create( &hiloAsignador, NULL, asignador, NULL);

/**************************************************     SECCION DEADLOCK      **********************************************************************/
	pthread_t hiloDeadlock;
	pthread_create( &hiloDeadlock, NULL, deadlock, NULL);

/***************************************************   SECCION CONEXIONES   ************************************************************************/
    addrlen = sizeof(struct sockaddr_in);
    while( (socketCliente = accept(socketEscucha, (struct sockaddr*)&cliente, (socklen_t*)&addrlen)) ) {
        if ( socketCliente == -1 ) {
            perror("accept");
            break;
        }
    	pthread_t nuevoEntrenador;
        if( pthread_create( &nuevoEntrenador, NULL, handshake, (void*) &socketCliente) )
            perror("pthread_create");
    }

/**************************************************   SECCION CIERRES   ****************************************************************************/
    nivel_gui_terminar();
    free(mapaMetadata);
    free(mapaMetadata->algoritmo);
    free(mapaMetadata->medalla);
    free(mapaMetadata->ip);
    free(pokemonMetadata);
	log_destroy(logger);

	list_destroy(eReady);
	list_destroy(eBlocked);
	list_destroy(eDeadlock);
	pthread_mutex_destroy(&mutexBlocked);
	pthread_mutex_destroy(&mutexReady);
	sem_destroy(&semAsignador);
	sem_destroy(&semPlanificador);
	system("setterm -cursor on");
	return EXIT_SUCCESS;
}
