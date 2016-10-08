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
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write & sleep

/* HILOS  */
#include <pthread.h>

/* COMMONS  */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/process.h>
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
t_list  *items;                         // Lista de elementos a graficar

char *rutaPokeDex;                      // Ruta del PokeDex

t_log *logger;                          // Log

/************************************************         FUNCIONES            *********************************************************************/
void obtenerMapaMetadata() {
	if ( getMapaMetadata(mapaMetadata, mapaMetadata->nombre, rutaPokeDex) ) {
		puts("No se encontro el mapa.");
		exit(1);
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

/* Función ordenadora para list_sort */
bool _menor_distancia(tEntrenador *entrenador, tEntrenador *entrenador_menor_distancia) {
    return distanciaObjetivo(entrenador, pokeNestArray) < distanciaObjetivo(entrenador_menor_distancia, pokeNestArray);
}

/************************************************  HILO I/0 ASIGNACION DE PKS  *********************************************************************/
void *asignador() {
	tEntrenador *entrenador;
	tPokemonMetadata *pokemon;
	int pos;
	char mensaje[128];
	while (1) {
		if (!list_is_empty(eBlocked)) {
			entrenador = (tEntrenador*)list_remove(eBlocked, 0);  // Saco al primero
		    log_info(logger, "Entrenador %c en proceso de asignación.", entrenador->id);
			pos = getPokeNestFromID(entrenador->obj);
			if(!queue_is_empty(pokeNestArray[pos]->pokemons)) {
				pokemon = (tPokemonMetadata*)queue_pop(pokeNestArray[pos]->pokemons);
				list_add(entrenador->pokemons, pokemon);
				restarRecurso(items, pokemon->id);

				sprintf(mensaje,"Pokemon %s Capturado!\n", pokeNestArray[pos]->nombre);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);

				sprintf(mensaje,"%s/Mapas/%s/PokeNest/%s/%s%03d.dat", rutaPokeDex, mapaMetadata->nombre, pokeNestArray[pos]->nombre, pokeNestArray[pos]->nombre, pokemon->ord);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);

				sprintf(mensaje,"Entrenador %c a cola de Ready.\n", entrenador->id);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);

				entrenador->obj = 0;
				list_add(eReady, entrenador);
				log_info(logger, "Pokemon %s capturado por entrenador %c!", pokeNestArray[pos]->nombre, entrenador->id);
				log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
			    printf("Pokemon %s capturado por entrenador %c!", pokeNestArray[pos]->nombre, entrenador->id);
				fflush(stdout);
				nivel_gui_dibujar(items, mapaMetadata->nombre);
			}
			else {
				sprintf(mensaje,"Lo siento se acabaron los %s. Entrenador %c cola de Bloqueados....\n", pokeNestArray[pos]->nombre, entrenador->id);
				send(entrenador->socket, mensaje, strlen(mensaje), 0);
				log_info(logger, "No hay mas %s, Entrenador %c agregado a la cola de Bloqueados.", pokeNestArray[pos]->nombre, entrenador->id);
				list_add(eBlocked, entrenador);
			}
		}
		sleep(5);
	}
}
/************************************************   PLANIFICADOR ROUND ROBIN   *********************************************************************/
void planRR() {
    int bRecibidos;
	int ql = mapaMetadata->quantum;          // Quantum restante
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];
    tEntrenador *entrenador = (tEntrenador*)list_remove(eReady, 0);  // Saco al primero
	log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (ql) {
		ql--;
		usleep( mapaMetadata->retardo * 1000 );
		bRecibidos = recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0);
		if (bRecibidos == 0) {
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
			return;
		}
		else if(bRecibidos == -1) {
			perror("recv");
			return;
		}
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			enviarCoordenadasEntrenador(entrenador, pokeNestArray, mensajeCliente[1]);
			break;
		/* MOVER AL ENTRENADOR */
		case 'M':
			moverEntrenador(items, entrenador, mensajeCliente[1], mapaMetadata->nombre);
			break;
		/* CAPTURAR EL POKEMON */
		case 'G':
			if (entregarPokemon(eBlocked, entrenador, pokeNestArray, mensajeCliente[1])) {
				nivel_gui_dibujar(items, mapaMetadata->nombre);
				return;
			}
			break;
		/* OBTENER MEDALLA */
		case 'O':
			send(entrenador->socket, mapaMetadata->medalla, strlen(mapaMetadata->medalla), 0);
			send(entrenador->socket, "\n", 1, 0);
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
			return;
		}
	}
	sprintf(mensajeServer,"Entrenador %c ha finalizado su Quantum!\n", entrenador->id);
	send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
	list_add(eReady, entrenador);
    log_info(logger, "Entrenador %c agregado a la cola de Listos.", entrenador->id);
}
/************************************************      PLANIFICADOR SRDF       *********************************************************************/
void planSRDF() {
	int bRecibidos;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];
    list_sort(eReady, (void*)_menor_distancia);                      // Ordeno la lista por menor distancia
    tEntrenador *entrenador = (tEntrenador*)list_remove(eReady, 0);  // Saco al primero
	log_info(logger, "Entrenador %c en ejecución.", entrenador->id);
	while (1) {
		usleep( mapaMetadata->retardo * 1000 );
		bRecibidos = recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0);
		if (bRecibidos == 0) {
			desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
			return;
		}
		else if(bRecibidos == -1) {
			perror("recv");
			return;
		}
		switch (mensajeCliente[0]) {
		/* OBTENER COORDENADA OBJETIVO */
		case 'C':
			if ( enviarCoordenadasEntrenador(entrenador, pokeNestArray, mensajeCliente[1]) ) {
				sprintf(mensajeServer,"Entrenador %c ha pasado a la cola de listos!\n", entrenador->id);
				send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
				list_add(eReady, entrenador);
				return;
			}
			break;
		/* MOVER AL ENTRENADOR */
		case 'M':
			moverEntrenador(items, entrenador, mensajeCliente[1], mapaMetadata->nombre);
			break;
		/* CAPTURAR EL POKEMON */
		case 'G':
			if (entregarPokemon(eBlocked, entrenador, pokeNestArray, mensajeCliente[1])) {
				nivel_gui_dibujar(items, mapaMetadata->nombre);
				return;
			}
			break;
		/* OBTENER MEDALLA */
		case 'O':
			send(entrenador->socket, mapaMetadata->medalla, strlen(mapaMetadata->medalla), 0);
			send(entrenador->socket, "\n", 1, 0);
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
/************************************************        HILO HANDSHAKE        *********************************************************************/
void *handshake(void *socket) {
    int bRecibidos;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];
	printf("Nueva Conexión!                                             ");
	fflush(stdout);
	nivel_gui_dibujar(items, mapaMetadata->nombre);
    tEntrenador *entrenador = malloc(sizeof(tEntrenador));
    entrenador->threadID = process_get_thread_id();
    entrenador->socket   = *(int*)socket;   //Castea el descriptor del socket
    entrenador->time     = time(0);
    entrenador->posx     = 1;
    entrenador->posy     = 1;
    entrenador->pokemons = list_create();

    sprintf(mensajeServer,"\nBienvenido al Mapa %s!\n\nEmpieza el juego!\n\nIngrese Simbolo Personaje: ", mapaMetadata->nombre);
    send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);

    bRecibidos = recv(entrenador->socket, mensajeCliente, 1, 0);
    if (bRecibidos == 0) {
		desconectarEntrenador(items, entrenador, pokeNestArray, mapaMetadata->nombre);
        return EXIT_SUCCESS;
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
        return EXIT_SUCCESS;
    }
    entrenador->id = mensajeCliente[0];
    CrearPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
    printf("Bienvenido Entrenador %c                                    ", entrenador->id);
    fflush(stdout);
    nivel_gui_dibujar(items, mapaMetadata->nombre);
	sprintf(mensajeServer,"\n\nBuena suerte %c!\n\nMR - Derecha\nML - Izquierda\nMU - Arriba\nMD - Abajo\n", entrenador->id);
	send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
	sprintf(mensajeServer,"\nCX - Coordenadas PokeNest X\nGX - Obtener Pokemon X\nO - Ruta Medalla del Mapa\n\n\n");
	send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
    list_add(eReady, entrenador);
	log_info(logger, "Entrenador %c Conectado!", entrenador->id);
	log_info(logger, "Entrenador %c a la cola de Listos.", entrenador->id);
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
	sprintf(mapaMetadata->medalla, "%s/Mapas/%s/medalla-%s.jpg", rutaPokeDex, mapaMetadata->nombre, mapaMetadata->nombre);

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
	eReady   = list_create();  // Lista de Listos
	eBlocked = list_create();  // Lista de Bloqueados

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
    return EXIT_SUCCESS;
}
