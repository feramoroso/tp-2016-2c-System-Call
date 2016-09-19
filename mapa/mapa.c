/*******************
    PROCESO MAPA
*******************/

/* STANDARD */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

/* SOCKETS */
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write

/* HILOS linkear con lpthread */
#include <pthread.h>

/* COMMONS */
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>

/* Libreria con funciones del Mapa */
#include "mapalib.h"

/* Librerias del graficador */
#include <curses.h>
#include "tad_items.h"

#define MAX_CON 10       // Conexiones máximas
#define T_NOM_HOST 128   // Tamaño del nombre del Server
#define TAM_MENSAJE 128  // Tamaño del mensaje Cliente/Servidor


/**********************
 * VARIABLES GLOBALES *
 *********************/
/*Metadatas*/
tMapaMetadata     *mapaMetadata;      // Estructura con la Metadaata del Mapa
tPokeNestMetadata *pokeNestArray[3];  // Arreglo con las distintas PokeNest
tPokemonMetadata  *pokemonMetadata;   // Estructura con la Metadaata de los Pokemon


t_list *items; //Lista de elementos a graficar


/********************************************
************* FUNCION DE HILO ***************
** Maneja las conexiones para cada cliente **
********************************************/
void *gestor_de_entrenadores(void *socket)
{
    //Castea el descriptor del socket
    int socketEntrenador = *(int*)socket;
    int bRecibidos;
    char simbolo;
    int posx, posy;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];

    //Manda mensaje al entrenador
    sprintf(mensajeServer,"Bienvenido al Mapa %s!\n", mapaMetadata->nombre);
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);
    sprintf(mensajeServer,"Respondeme el saludo:\n");
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);

    //Recibe el saludo del entrenador, cuando recv devuelve 0 o negativo termina por desconexion o error
    bRecibidos = recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0);
    printf("Cantidad de Bytes recibidos : %d\n", bRecibidos);
    mensajeCliente[bRecibidos] = '\0';
    printf("Entrenador %d: %s\n", socketEntrenador, mensajeCliente);

    sprintf(mensajeServer,"\nEmpieza el juego!\n\n Ingrese Simbolo Personaje: \n\n 1 - Derecha\n 2 - Izquierda\n 3 - Arriba\n 4 - Abajo\n");
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);

    recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0);

    simbolo = mensajeCliente[0];
    CrearPersonaje(items, simbolo, 1, 1);
    nivel_gui_dibujar(items, mapaMetadata->nombre);

    getchar();
    /*
    //Recibe mensajes del entrenador, cuando recv devuelve 0 o negativo termina por desconexion o error
    while ((bRecibidos = recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0)) > 0 ) {
        printf("Cantidad de Bytes recibidos : %d\n", bRecibidos);
        mensajeCliente[bRecibidos] = '\0';
    	printf("Entrenador %d: %s\n", socketEntrenador, mensajeCliente);
    }*/



    if (bRecibidos == 0) {
        printf("Entrenador %d Desconectado!\n", socketEntrenador);
        fflush(stdout);
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
    }

    BorrarItem(items, simbolo);
    nivel_gui_terminar();
    free(socket);
    return EXIT_SUCCESS;
}

int getPokeNestArray(char *nomMapa, char *rutaPokeDex) {
	int i = 0;
	DIR *dir;
	struct dirent *dirPokeNest;
	char ruta[256];
	sprintf(ruta, "%s/Mapas/%s/PokeNests/", rutaPokeDex, nomMapa);
	dir = opendir(ruta);
	while ((dirPokeNest = readdir(dir))) {
		if ( (dirPokeNest->d_type == DT_DIR) && (strcmp(dirPokeNest->d_name, ".")) && (strcmp(dirPokeNest->d_name, "..")) ) {

			if ( (pokeNestArray[i] = getPokeNestMetadata(nomMapa, dirPokeNest->d_name, rutaPokeDex)) == NULL) {
				closedir(dir);
				return EXIT_FAILURE;
			}
			printf("\nNombre de la PokeNest: %s", pokeNestArray[i]->nombre);
			printf("\nTipo:                  %s", pokeNestArray[i]->tipo);
			printf("\nPosición en x:         %d", pokeNestArray[i]->posx);
			printf("\nPosición en y:         %d", pokeNestArray[i]->posy);
			printf("\nIdentificador:         %c\n\n", pokeNestArray[i]->ID);
			i++;
		}
	}
	closedir(dir);
	return EXIT_SUCCESS;
}

/********************************************
******************* MAIN*********************
********************************************/
int main(int argc , char *argv[]) {
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: mapa <nombre_mapa> <ruta PokeDex>");
		return EXIT_FAILURE;
	}

	//t_log *log = log_create(PATH_LOG_MAP, argv[1], true, 3);
	strcpy(argv[2],RUTA_POKEDEX);
	/* OBTENER METADATA DEL MAPA */
	mapaMetadata = getMapaMetadata(argv[1],argv[2]);
	if (mapaMetadata == NULL) {
		puts("\nNo se encontro el mapa.");
		return EXIT_FAILURE;
	}

	if (getPokeNestArray(mapaMetadata->nombre, argv[2])) {
		puts("\nNo se encontro la PokeNest.");
		return EXIT_FAILURE;
	}

	/* OBTENER METADATA DE LAS POKENESTS *//*
	pokeNestMetadata = getPokeNestMetadata(mapaMetadata->nombre, "Pikachu", argv[2]);
	if (pokeNestMetadata == NULL) {
		puts("No se encontro la PokeNest.");
		return EXIT_FAILURE;
	}*/

	/* OBTENER METADATA DE LOS POKEMON */
	pokemonMetadata = getPokemonMetadata(mapaMetadata->nombre, "Pikachu", 1, argv[2]);
	if (pokemonMetadata == NULL) {
		puts("\nNo se encontro el Pokemon.");
		return EXIT_FAILURE;
	}

	/* SECCION SOCKETS */
	int socketEscucha , socketCliente , *socketNuevo;
	int addrlen; // Tamaño de Bytes aceptados por accept
    struct sockaddr_in server , cliente;
	char hostname[T_NOM_HOST]; // Cadena para almacenar el nombre del host del Server

	/* Creacion del Socket TCP */
    socketEscucha = socket(AF_INET , SOCK_STREAM , 0);
    if (socketEscucha == -1) {
        puts("No se pudo crear el socket.");
        return EXIT_FAILURE;
    }

    puts("Socket creado!\n");


    /* Preparando la estructura */
	server.sin_family = AF_INET;
	server.sin_port = htons(mapaMetadata->puerto);   // Puerto extraido del archivo metadata
	inet_aton(mapaMetadata->ip, &(server.sin_addr)); // IP extraida del archivo metadata
	//server.sin_addr.s_addr = htonl(INADDR_ANY);    // Mi propia dirección IP
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
    listen(socketEscucha , MAX_CON);
    puts("Esperando entrenadores...");
    getchar();



    /*******************
     * Grafico el mapa
     *******************/
    items = list_create();
    int i = 0;
    while ( pokeNestArray[i]) {
    	CrearCaja(items, pokeNestArray[i]->ID, pokeNestArray[i]->posx, pokeNestArray[i]->posy, 5);
    	i++;
    }

    nivel_gui_inicializar();
    int finx, finy;
    nivel_gui_get_area_nivel(&finx, &finy);
    nivel_gui_dibujar(items, mapaMetadata->nombre);

    addrlen = sizeof(struct sockaddr_in);
    while( (socketCliente = accept(socketEscucha, (struct sockaddr *)&cliente, (socklen_t*)&addrlen)) ) {
        puts("Conexión Aceptada!");

        pthread_t nuevoHilo;
        socketNuevo = malloc(1);
        *socketNuevo = socketCliente;

        if( pthread_create( &nuevoHilo, NULL,  gestor_de_entrenadores, (void*) socketNuevo) ) {
            perror("No se pudo crear el hilo para el Entrenador.");
            return EXIT_FAILURE;
        }

        puts("Nuevo Hilo en Ejecución.");
        //Por el momento no vamos a esperar que termine
        //pthread_join( nuevo_hilo , NULL);
    }

    if (socketCliente == -1 ) {
        perror("No se pudo Aceptar la conexión.");
        return EXIT_FAILURE;
    }

    free(mapaMetadata);
    //free(pokeNestArray);
    free(pokemonMetadata);
	//log_destroy(log);
    return EXIT_SUCCESS;
}
