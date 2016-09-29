/*******************
    PROCESO MAPA
*******************/

/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* SOCKETS */
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write & sleep

/* HILOS linkear con lpthread */
#include <pthread.h>

/* COMMONS linkear con lcommons  */
#include <commons/error.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/process.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

#define MAX_CON     10     // Conexiones máximas
#define T_NOM_HOST  128    // Tamaño del nombre del Server
#define TAM_MENSAJE 128    // Tamaño del mensaje Cliente/Servidor

/***************************************************************************************************************************************************/
/**********************************************  VARIABLES GLOBALES  *******************************************************************************/
/***************************************************************************************************************************************************/
/*Metadatas*/
tMapaMetadata     *mapaMetadata;       // Estructura con la Metadaata del Mapa
tPokeNestMetadata *pokeNestArray[10];  // Arreglo con las distintas PokeNest
tPokemonMetadata  *pokemonMetadata;    // Estructura con la Metadaata de los Pokemon

t_list  *eReady;                      // Lista de Entrenadores Listos
t_list  *eBlocked;                    // Lista de Entrenadores Bloqueados

t_list  *items;                       // Lista de elementos a graficar

char rutaMedalla[256];                 // Ruta del archivo de la medalla del Mapa


/************************************************         FUNCIONES            *********************************************************************/
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


/************************************************ HILO PLANIFICADOR ROUND ROBIN  *******************************************************************/
void *planRR() {
    int ql;               // Quantum restante
    int bRecibidos;
    int auxPos, auxX, auxY, distancia;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];
    tEntrenador *entrenador;

    while(1) {
		if (!list_is_empty(eReady)) {
    		ql = mapaMetadata->quantum;
    		entrenador = (tEntrenador*)list_remove(eReady, 0);

			sprintf(mensajeServer,"\n\nBuena suerte %c!\n\nMR - Derecha\nML - Izquierda\nMU - Arriba\nMD - Abajo\n", entrenador->id);
			send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
			sprintf(mensajeServer,"\nCX - Coordenadas PokeNest X\nGX - Obtener Pokemon X\nO - Ruta Medalla del Mapa\n\n\n");
			send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);

			while (ql) {
				bRecibidos = recv(entrenador->socket, mensajeCliente, TAM_MENSAJE, 0);
				if (bRecibidos == 0) {
					printf("Entrenador %c Desconectado!", entrenador->id);
					fflush(stdout);
					devolverPokemons(items, entrenador, pokeNestArray);
					BorrarItem(items, entrenador->id);
					nivel_gui_dibujar(items, mapaMetadata->nombre);
					close(entrenador->socket);
					free(entrenador);
					ql = 0;
				}
				else if(bRecibidos == -1) {
					perror("Error de Recepción");
				}
				else {
					switch (mensajeCliente[0]) {
					/* OBTENER COORDENADA OBJETIVO */
					case 'C':
						entrenador->obj = mensajeCliente[1];
						auxPos = getPokeNestFromID(mensajeCliente[1]);
						if (pokeNestArray[auxPos] != NULL) {
							auxX = pokeNestArray[auxPos]->posx;
							auxY = pokeNestArray[auxPos]->posy;
							distancia = distanciaObjetivo(entrenador, pokeNestArray);
							sprintf(mensajeServer,"%3d%3d\nDistancia: %d\n", auxX, auxY, distancia);
						}
						else
							sprintf(mensajeServer,"No existe la PokeNest\n");
						send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
						break;
					/* MOVER AL ENTRENADOR */
					case 'M':
						switch (mensajeCliente[1]) {
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
						break;
					/* CAPTURAR EL POKEMON */
					case 'G':
						if (entrenador->obj == mensajeCliente[1]) {
							auxPos = getPokeNestFromID(mensajeCliente[1]);
							if (pokeNestArray[auxPos] != NULL) {
								distancia = distanciaObjetivo(entrenador, pokeNestArray);
								if (!queue_is_empty(pokeNestArray[auxPos]->pokemons)) {
									if(distancia == 0) {
										list_add(entrenador->pokemons, queue_pop(pokeNestArray[auxPos]->pokemons));
										restarRecurso(items, pokeNestArray[auxPos]->id);
										sprintf(mensajeServer,"Pokemon %s Capturado!\n", pokeNestArray[auxPos]->nombre);
										ql = 1;
									}
									else
										sprintf(mensajeServer,"Aun se encuentra a %d de la PokeNest %s!\n", distancia, pokeNestArray[auxPos]->nombre);
								}
								else
									sprintf(mensajeServer,"Lo siento se acabaron los %s. A cola de Bloqueados....\n", pokeNestArray[auxPos]->nombre);
							}
							else
								sprintf(mensajeServer,"No existe la PokeNest!\n");
						}
						else
							sprintf(mensajeServer,"Solicitar el objetivo primero!\n");
						send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
						break;
					/* OBTENER MEDALLA */
					case 'O':
						send(entrenador->socket, rutaMedalla, strlen(rutaMedalla), 0);
						send(entrenador->socket, "\n", 1, 0);
						break;
					}
					MoverPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
					nivel_gui_dibujar(items, mapaMetadata->nombre);
					ql--;
					if (!ql) {
						sprintf(mensajeServer,"Entrenador %c has sido Bloquedo!\n", entrenador->id);
						send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);
						list_add(eReady, entrenador);
					}
				}
			}
    	}
    }
}

/************************************************    HILO PLANIFICADOR SRDF      *******************************************************************/
void *planSRDF() {

}
/********************************************
************* FUNCION DE HILO ***************
** Maneja las conexiones para cada cliente **
********************************************/
void *handshake(void *socket) {
    int bRecibidos;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];

    tEntrenador *entrenador = malloc(sizeof(tEntrenador));
    entrenador->threadID = process_get_thread_id();
    entrenador->socket   = *(int*)socket;   //Castea el descriptor del socket
    entrenador->time     = time(0);
    entrenador->posx     = 1;
    entrenador->posy     = 1;
    entrenador->pokemons = list_create();

    //Manda mensaje al entrenador
    nivel_gui_dibujar(items, mapaMetadata->nombre);
    write(0,"Nueva conexión!                         ",40);
    sprintf(mensajeServer,"\nBienvenido al Mapa %s!\n\nEmpieza el juego!\n\nIngrese Simbolo Personaje: ", mapaMetadata->nombre);
    send(entrenador->socket, mensajeServer, strlen(mensajeServer), 0);

    bRecibidos = recv(entrenador->socket, mensajeCliente, 1, 0);
    if (bRecibidos == 0) {
    	nivel_gui_dibujar(items, mapaMetadata->nombre);
    	printf("Entrenador %d Desconectado!", entrenador->socket);
        fflush(stdout);
        return EXIT_SUCCESS;
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
        return EXIT_SUCCESS;
    }
    entrenador->id = mensajeCliente[0];
    CrearPersonaje(items, entrenador->id, entrenador->posx, entrenador->posy);
    nivel_gui_dibujar(items, mapaMetadata->nombre);
    list_add(eReady, entrenador);
}

/***************************************************************************************************************************************************/
/**************************************************   MAIN   ***************************************************************************************/
/***************************************************************************************************************************************************/
int main(int argc , char *argv[]) {
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: mapa <nombre_mapa> <ruta PokeDex>");
		return EXIT_FAILURE;
	}

	//t_log *log = log_create(PATH_LOG_MAP, argv[1], true, 3);

/**************************************************   SECCION METADATA   ***************************************************************************/
	/* OBTENER METADATA DEL MAPA */
	mapaMetadata = getMapaMetadata(argv[1],argv[2]);
	if (mapaMetadata == NULL) {
		error_show("No se encontro el mapa.\n");
		//puts("\nNo se encontro el mapa.");
		return EXIT_FAILURE;
	}
	/* OBTENER ARREGLO DE POKENEST */
	if (getPokeNestArray(pokeNestArray, mapaMetadata->nombre, argv[2])) {
		error_show("PokeNest invalida.\n");
		return EXIT_FAILURE;
	}
	/* OBTENER COLA DE POKEMONS EN CADA POKENEST */
	if (getPokemonsQueue(pokeNestArray, mapaMetadata->nombre, argv[2])) {
		error_show("Error en la PokeNest.\n");
		return EXIT_FAILURE;
	}
	/* OBTENER LA RUTA DE LA MEDALLA */
	sprintf(rutaMedalla,"%s/Mapas/%s/medalla-%s.jpg",argv[2],argv[1],argv[1]);

	imprimirInfoPokeNest(pokeNestArray);

	/* LISTAS DE ENTRENADORES */
	eReady   = list_create();
	eBlocked = list_create();
/**************************************************   SECCION SOCKETS   ****************************************************************************/
	int socketEscucha , socketCliente , *socketNuevo;
	int addrlen;                                       // Tamaño de Bytes aceptados por accept
    struct sockaddr_in server , cliente;
	char hostname[T_NOM_HOST];                         // Cadena para almacenar el nombre del host del Server

	/* Creacion del Socket TCP */
    socketEscucha = socket(AF_INET, SOCK_STREAM, 0);
    if (socketEscucha == -1) {
    	error_show("No se pudo crear el socket.\n");
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
        perror("bind Fallo. Error");
        return EXIT_FAILURE;
    }
    puts("Bind exitoso!");

    /* Pongo a escuchar el Socket con listen() */
    listen(socketEscucha, MAX_CON);

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
    write(0,"Esperando entrenadores...",25);

/**************************************************   SECCION PLANIFICACION   **********************************************************************/
	pthread_t planificador;
	if ( !strncmp(mapaMetadata->algoritmo, "RR", 2) )
		pthread_create( &planificador, NULL, planRR, NULL);
	else if (!strncmp(mapaMetadata->algoritmo,"SRDF", 4))
		pthread_create( &planificador, NULL, planSRDF, NULL);

/***************************************************   SECCION CONEXIONES   ************************************************************************/
    addrlen = sizeof(struct sockaddr_in);
    while( (socketCliente = accept(socketEscucha, (struct sockaddr *)&cliente, (socklen_t*)&addrlen)) ) {
        pthread_t nuevoHilo;
        socketNuevo = malloc(1);
        *socketNuevo = socketCliente;
        if( pthread_create( &nuevoHilo, NULL, handshake, (void*) socketNuevo) ) {
            perror("No se pudo crear el hilo para el Entrenador.");
            return EXIT_FAILURE;
        }
    }
    nivel_gui_terminar();
    if (socketCliente == -1 ) {
        perror("No se pudo Aceptar la conexión.");
        return EXIT_FAILURE;
    }

/**************************************************   SECCION CIERRES   ****************************************************************************/
    free(mapaMetadata);
    //free(pokeNestArray);
    free(pokemonMetadata);
	//log_destroy(log);
    return EXIT_SUCCESS;
}
