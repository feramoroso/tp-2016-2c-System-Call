/***************************************************************************************************************************************************/
/************************************************         PROCESO MAPA         *********************************************************************/
/***************************************************************************************************************************************************/

/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

/* SOCKETS */
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

/* HILOS  */
#include <pthread.h>

/* COMMONS  */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/process.h>
#include <commons/error.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

/* PKMN BATTLE */
#include <pkmn/battle.h>
#include <pkmn/factory.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

#define MAX_CON     10     // Conexiones máximas
#define T_NOM_HOST  128    // Tamaño del nombre del Server
#define TAM_MENSAJE 128    // Tamaño del mensaje Cliente/Servidor

/***************************************************************************************************************************************************/
/************************************************      VARIABLES GLOBALES      *********************************************************************/
/***************************************************************************************************************************************************/
tMapaMetadata     *mapaMetadata;        // Estructura con la Metadaata del Mapa
tPokeNestMetadata *pokeNestArray[100];  // Arreglo con las distintas PokeNest
tPokemonMetadata  *pokemonMetadata;     // Estructura con la Metadaata de los Pokemon

t_list  *eReady;                        // Lista de Entrenadores Listos
t_list  *eBlocked;                      // Lista de Entrenadores Bloqueados
t_list  *eDeadlock;                     // Lista de Entrenadores Bloqueados
t_list  *items;                         // Lista de elementos a graficar

char *rutaPokeDex;                      // Ruta del PokeDex

pthread_mutex_t mutexBlocked = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutexReady   = PTHREAD_MUTEX_INITIALIZER;

t_log *logger;                          // Log

char matPedidos[100][100];              // Matriz de Recursos Solicitados por los entrenadores bloqueados
char matAsignados[100][100];            // Matriz de Recursos Asignados a los entrenadores bloqueados
char vecDisponibles[100];               // Vector de Recursos Disponibles

/***************************************************************************************************************************************************/
/************************************************         FUNCIONES            *********************************************************************/
/***************************************************************************************************************************************************/
void obtenerMapaMetadata() {
	if ( getMapaMetadata(mapaMetadata, mapaMetadata->nombre, rutaPokeDex) ) {
		puts("No se encontro el mapa.");
		exit(EXIT_FAILURE);
	}
}
/* Obtiene la posicion de la PokeNest id en el pokeNestArray */
int getPokeNestFromID(char id) {
	int i = 0;
	while (pokeNestArray[i]) {
		if (pokeNestArray[i]->id == id)
			return i;
		i++;
	}
	return i;
}
/************************************************    FUNCIONES ORDENADORAS     *********************************************************************/
bool _menor_distancia(tEntrenador *entrenador, tEntrenador *entrenador_menor_distancia) {
    return distanciaObjetivo(entrenador, pokeNestArray) < distanciaObjetivo(entrenador_menor_distancia, pokeNestArray);
}
bool _mayor_tiempo(tEntrenador *entrenador, tEntrenador *entrenador_mayor_tiempo) {
    return entrenador->time > entrenador_mayor_tiempo->time;
}
bool _mayor_nivel(tPokemonMetadata *pokemon, tPokemonMetadata *pokemon_mayor_nivel) {
    return pokemon->data->level > pokemon_mayor_nivel->data->level;
}
/************************************************   FUNCIONES PLANIFICADOR     *********************************************************************/
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
void enviarCoordenadasEntrenador(tEntrenador *entrenador, char pokeNest) {
	int pos = getPokeNestFromID(pokeNest);
	entrenador->obj = pokeNest;
	send(entrenador->socket, string_from_format("%3d%3d", pokeNestArray[pos]->posx, pokeNestArray[pos]->posy), 6, 0);
	printf("Entrenador %c en busqueda de Pokemon %s.                    ", entrenador->id, pokeNestArray[pos]->nombre);
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
}
void solicitarPokemon(tEntrenador *entrenador, char pokeNest) {
	int pos;
	pos = getPokeNestFromID(pokeNest);
	pthread_mutex_lock(&mutexBlocked);
	list_add(eBlocked, entrenador);
	pthread_mutex_unlock(&mutexBlocked);
	printf("Entrenador %c ha solicitado Pokemon %s.                     ", entrenador->id, pokeNestArray[pos]->nombre);
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
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
};
void llenarEstructuras(int eMax, int pMax) {
	int i, j, k;
	tEntrenador *entrenador;
	tPokemonMetadata *pokemon;
	for(i = 0; i < eMax; i++) {
		entrenador = (tEntrenador*)list_get(eBlocked, i);
		matPedidos[i][getPokeNestFromID(entrenador->obj)] = 1;
		for(j = 0; j < list_size(entrenador->pokemons); j++) {
			pokemon = (tPokemonMetadata*)list_get(entrenador->pokemons, j);
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
    tablas = fopen("log.txt","a");
	int i,j;
    fprintf(tablas, "\nEntrenador   -   Asignados   -   Pedidos   -   Disponibles");
	for(i = 0; i < eMax; i++)
	{
	fprintf(tablas, "\n    %c            ", ((tEntrenador*)list_get(eBlocked, i))->id);
		for(j=0;j<pMax;j++)
		{
			fprintf(tablas, "%d ", matAsignados[i][j]);
		}
		fprintf(tablas, "          ");
		for(j=0;j<pMax;j++)
		{
			fprintf(tablas, "%d ", matPedidos[i][j]);
		}
		fprintf(tablas, "        ");
		if(i==0)
		{
			for(j=0;j<pMax;j++)
				fprintf(tablas, "%d ", vecDisponibles[j]);
		}
	}
	fprintf(tablas, "\n\n");
	fclose(tablas);
}
int  deadlockDetect(int eMax, int pMax) {
	tEntrenador *entrenador;
	int m[100], vecTemp[100],
		found, flag,
		i, j, k, l, sum;

	k = 0;
	for(i = 0; i < eMax; i++) {
		sum = 0;
		for(j = 0; j < pMax; j++) {
			sum += matAsignados[i][j];
		}
		if(sum == 0) {
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
		if(flag == 1) {
			m[k] = i;
			k++;
			for(j = 0; j < pMax; j++)
				vecTemp[j] += matAsignados[i][j];
		}
	}
	/* Procesos en Deadlock */
	for(i = 0; i < eMax; i++) {
		found = 0;
		for(j = 0; j < k; j++) {
			if(i == m[j])
				found = 1;
		}
		if(found == 0) {
			entrenador = list_get(eBlocked, i);
			/* Le aviso al entrenador que esta en Deadlock */
			send(entrenador->socket, "D", 1, 0);
			list_add(eDeadlock, entrenador);
		}
	}
	if(!found) {
		log_warning(logger, "Interbloqueo Detectado!");
		imprimirEstructuras(eMax, pMax);
		return 1;
	}
	return 0;
}
void batallaPokemon() {
	tEntrenador *eA, *eB;
	tPokemonMetadata *pkA, *pkB, *pkLoser;
	char mensaje[256];
	int i;

	pkLoser = malloc(sizeof(tPokemonMetadata));
	list_sort(eDeadlock, (void*)_mayor_tiempo);                     // Ordeno la lista por mayor tiempo
	while( list_size(eDeadlock) > 1 ) {

		eA = list_remove(eDeadlock, 0);                             // Saco el primer entrenador de la Lista de Deadlock
		list_sort(eA->pokemons, (void*)_mayor_nivel);               // Ordeno los pokemons por mayor nivel
		pkA = (tPokemonMetadata*)list_get(eA->pokemons, 0);                            // Tomo al pokemon de mayor nivel

		eB = list_remove(eDeadlock, 0);                             // Saco el segundo entrenador de la Lista de Deadlock
		list_sort(eB->pokemons, (void*)_mayor_nivel);               // Ordeno los pokemons por mayor nivel
		pkB = (tPokemonMetadata*)list_get(eB->pokemons, 0);                            // Tomo al pokemon de mayor nivel

		sprintf(mensaje, "****  Batalla Pokemon! -  %c  Vs.  %c  ****", eA->id, eB->id);
	    log_info(logger, "%s", mensaje);

	    pkLoser->data = pkmn_battle(pkA->data, pkB->data);          // Batalla!

		if(pkLoser->data == pkB->data) {
	    	/* Al entrenador lo vuelvo a ingresar a la lista de Deadlock para la proxima batalla */
			sprintf(mensaje, "****   Ganador  %c  -  Perdedor  %c   ****", eA->id, eB->id);
			list_add(eDeadlock, eB);
	    	/* Le aviso a el entrenador ganador que zafo */
	    	send(eA->socket, "R", 1, 0);

		} else {
			/* Al entrenador que perdio lo vuelvo a ingresar a la lista de Deadlock para la proxima batalla */
			sprintf(mensaje, " ****   Ganador  %c  -  Perdedor  %c   ****\n", eB->id, eA->id);
			list_add(eDeadlock, eA);
	    	/* Le aviso a el entrenador ganador que zafo */
	    	send(eB->socket, "R", 1, 0);
		}
	    log_info(logger, "%s", mensaje);
	    if ( list_size(eDeadlock) == 1) {
	    	eA = (tEntrenador*)list_remove(eDeadlock, 0);
	    	i = 0;
	    	while(eA != (tEntrenador*)list_get(eBlocked, i))
	    		i++;
	    	/* Le aviso a el entrenador que perdio */
	    	send(eA->socket, "K", 1, 0);

	    	sprintf(mensaje, "Entrenador %c ha perdido la batala Pokemon!", eA->id);
	    	log_info(logger, "%s", mensaje);
	    	list_remove(eBlocked, i);
	    	desconectarEntrenador(items, eA, pokeNestArray, mapaMetadata->nombre);
	    }
	}
}
/************************************************        HILO HANDSHAKE        *********************************************************************/
void *handshake(void *socket) {
    int bRecibidos;
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

    sprintf(mensajeServer,"\n******  Bienvenido a %s ******\n\n", mapaMetadata->nombre);
    send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);

    bRecibidos = recv(entrenador->socket, mensajeCliente, 1, 0);
    if (bRecibidos < 1) {
		desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
        return EXIT_SUCCESS;
    }
    entrenador->id = mensajeCliente[0];
    CrearPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
    printf("Bienvenido Entrenador %c                                    ", entrenador->id);
    fflush(stdout);
    nivel_gui_dibujar(items, mapaMetadata->nombre);
    list_add(eReady, entrenador);
	log_info(logger, "Entrenador %c Conectado!", entrenador->id);
	log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
}
/************************************************  HILO I/0 ASIGNACION DE PKS  *********************************************************************/
void *asignador() {
	tEntrenador *entrenador;
	tPokemonMetadata *pokemon;
	int pos;
	char mensaje[256];
	while (1) {
		pthread_mutex_lock(&mutexBlocked);
		if (!list_is_empty(eBlocked)) {
			entrenador = (tEntrenador*)list_remove(eBlocked, 0);
			log_info(logger, "Entrenador %c en proceso de asignación.", entrenador->id);
			pos = getPokeNestFromID(entrenador->obj);
			if(!queue_is_empty(pokeNestArray[pos]->pokemons)) {
				pokemon = (tPokemonMetadata*)queue_pop(pokeNestArray[pos]->pokemons);
				list_add(entrenador->pokemons, pokemon);
				restarRecurso(items, pokemon->id);
				sprintf(mensaje,"%s/Mapas/%s/PokeNests/%s/%s%03d.dat", rutaPokeDex, mapaMetadata->nombre, pokeNestArray[pos]->nombre, pokeNestArray[pos]->nombre, pokemon->ord);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);
				entrenador->obj = 0;
				pthread_mutex_lock(&mutexReady);
				list_add(eReady, entrenador);
				pthread_mutex_unlock(&mutexReady);
				log_info(logger, "Pokemon %s capturado por entrenador %c!", pokeNestArray[pos]->nombre, entrenador->id);
				log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
			    printf("Pokemon %s capturado por entrenador %c!                     ", pokeNestArray[pos]->nombre, entrenador->id);
				fflush(stdout);
				nivel_gui_dibujar(items, mapaMetadata->nombre);
			}
			else {
				log_info(logger, "No hay mas %s, Entrenador %c agregado a la cola de Bloqueados.", pokeNestArray[pos]->nombre, entrenador->id);
				list_add(eBlocked, entrenador);
			}
		}
		pthread_mutex_unlock(&mutexBlocked);
		sleep(1);
	}
}
/************************************************   PLANIFICADOR ROUND ROBIN   *********************************************************************/
void planRR() {
    int bRecibidos;
	int ql = mapaMetadata->quantum;          // Quantum restante
    char mensajeCliente[TAM_MENSAJE];
    tEntrenador *entrenador = (tEntrenador*)list_remove(eReady, 0);
	log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (ql) {
		ql--;
		usleep( mapaMetadata->retardo * 1000 );
		bRecibidos = recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0);
	    if (bRecibidos < 1) {
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
	        return;
	    }
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			enviarCoordenadasEntrenador(entrenador, mensajeCliente[1]);
			break;
		/* MOVER AL ENTRENADOR */
		case 'M':
			moverEntrenador(entrenador, mensajeCliente[1]);
			break;
		/* CAPTURAR EL POKEMON */
		case 'G':
			solicitarPokemon(entrenador, mensajeCliente[1]);
			return;
			break;
		/* OBTENER MEDALLA */
		case 'O':
			send(entrenador->socket, mapaMetadata->medalla, strlen(mapaMetadata->medalla), 0);
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
			return;
		}
	}
	list_add(eReady, entrenador);
    log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
}
/************************************************      PLANIFICADOR SRDF       *********************************************************************/
void planSRDF() {
	int bRecibidos;
    char mensajeCliente[TAM_MENSAJE];
    pthread_mutex_lock(&mutexReady);
    list_sort(eReady, (void*)_menor_distancia);                      // Ordeno la lista por menor distancia
    tEntrenador *entrenador = (tEntrenador*)list_remove(eReady, 0);  // Saco al primero
    pthread_mutex_unlock(&mutexReady);
    log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (1) {
		usleep( mapaMetadata->retardo * 1000 );
		bRecibidos = recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0);
	    if (bRecibidos < 1) {
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
	        return;
	    }
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			enviarCoordenadasEntrenador(entrenador, mensajeCliente[1]);
			log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
			pthread_mutex_lock(&mutexReady);
			list_add(eReady, entrenador);
			pthread_mutex_unlock(&mutexReady);
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
			break;
		/* OBTENER MEDALLA */
		case 'O':
			send(entrenador->socket, mapaMetadata->medalla, strlen(mapaMetadata->medalla), 0);
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
			return;
		}
	}
}
/************************************************      HILO PLANIFICADOR       *********************************************************************/
void *planificador() {
	while (1) {
		if (!list_is_empty(eReady)) {
		    if      ( !strncmp(mapaMetadata->algoritmo, "RR"  , 2) )
				planRR();
			else if ( !strncmp(mapaMetadata->algoritmo, "SRDF", 4) )
				planSRDF();
		}
	}
}
/************************************************        HILO DEADLOCK         *********************************************************************/
void *deadlock() {
	int i = 0, eMax, pMax;
	while (pokeNestArray[i])
		i++;
	pMax = i;
	while(1) {
		usleep( mapaMetadata->tiempoDeadlock * 1000 );
		pthread_mutex_lock(&mutexBlocked);
		if ( !list_is_empty(eBlocked) ) {
			eMax = list_size(eBlocked);
			inicializarEstructuras(eMax, pMax);
			llenarEstructuras(eMax, pMax);
			if ( deadlockDetect(eMax, pMax) && mapaMetadata->batalla )
				batallaPokemon();
		}
		pthread_mutex_unlock(&mutexBlocked);
	}
}
/***************************************************************************************************************************************************/
/************************************************             MAIN             *********************************************************************/
/***************************************************************************************************************************************************/
int main(int argc , char *argv[]) {
	system("clear");
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: mapa <nombre_mapa> <ruta PokeDex>");
		return EXIT_FAILURE;
	}
/**************************************************   SECCION METADATA   ***************************************************************************/
	/* OBTENER METADATA DEL MAPA */
	mapaMetadata         = malloc(sizeof(tMapaMetadata));
	mapaMetadata->nombre = argv[1];
	rutaPokeDex          = argv[2];
	obtenerMapaMetadata();

	/* OBTENER LA RUTA DE LA MEDALLA */
	mapaMetadata->medalla = string_from_format("%s/Mapas/%s/medalla-%s.jpg", rutaPokeDex, mapaMetadata->nombre, mapaMetadata->nombre);

	imprimirInfoMapa(mapaMetadata);

	/* OBTENER ARREGLO DE POKENEST */
	if (getPokeNestArray(pokeNestArray, mapaMetadata->nombre, rutaPokeDex)) {
		error_show("PokeNest invalida.\n");
		return EXIT_FAILURE;
	}
	/* OBTENER COLA DE POKEMONS EN CADA POKENEST */
	if (getPokemonsQueue(pokeNestArray, mapaMetadata->nombre, rutaPokeDex)) {
		error_show("Error en la PokeNest.\n");
		return EXIT_FAILURE;
	}

	imprimirInfoPokeNest(pokeNestArray);

	/* LISTAS DE ENTRENADORES */
	eReady    = list_create();  // Lista de Entrenadores Listos para Ejecucíon
	eBlocked  = list_create();  // Lista de Entrenadores Bloqueados para Captura de Pokemons
	eDeadlock = list_create();  // Lista de Entrenadores en Interbloqueo

	pthread_mutex_init(&mutexBlocked, NULL);
	pthread_mutex_init(&mutexReady,   NULL);

	/* LOG */
	logger = log_create("log.txt", mapaMetadata->nombre, false, LOG_LEVEL_INFO);

/**************************************************   SECCION SEÑALES   ****************************************************************************/
	signal(SIGUSR2, obtenerMapaMetadata);

/**************************************************   SECCION SOCKETS   ****************************************************************************/
	int socketEscucha , socketCliente;
	int addrlen;                                       // Tamaño de Bytes aceptados por accept
    struct sockaddr_in server , cliente;
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

	/* Enlace usando Bind() */
    if( bind(socketEscucha,(struct sockaddr *)&server , sizeof(server)) == -1) {
        perror("bind");
        return EXIT_FAILURE;
    }
    puts("Bind exitoso!");

    /* Pongo a escuchar el Socket con listen() */
    if( listen(socketEscucha, MAX_CON) == -1) {
    	perror("listen");
    	return EXIT_FAILURE;
    }
    puts("Escuchando conexiones...");

/**************************************************   SECCION GRAFICO   ****************************************************************************/
    puts("Presione Enter para iniciar el modo gráfico...");
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
    printf("Esperando entrenadores...                                   ");
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
    	pthread_t nuevoHilo;
        if( pthread_create( &nuevoHilo, NULL, handshake, (void*) &socketCliente) )
            perror("pthread_create");
    }

/**************************************************   SECCION CIERRES   ****************************************************************************/
    nivel_gui_terminar();
    free(mapaMetadata);
    //free(pokeNestArray);
    free(pokemonMetadata);
	log_destroy(logger);

	list_destroy(eReady);
	list_destroy(eBlocked);
	list_destroy(eDeadlock);

	pthread_mutex_destroy(&mutexBlocked);
	pthread_mutex_destroy(&mutexReady);
    return EXIT_SUCCESS;
}
