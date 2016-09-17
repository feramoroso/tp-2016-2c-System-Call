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

#define MAX_CON 10     // Conexiones máximas
#define T_NOM_HOST 128 // Tamaño del nombre del Server
#define TAM_MENSAJE 128  // Tamaño del mensaje Cliente/Servidor

/*
 * FUNCION DE HILO
 * Maneja las conexiones para cada cliente
 * */
void *gestor_de_entrenadores(void *socket)
{
    //Castea el descriptor del socket
    int socketEntrenador = *(int*)socket;
    int bRecibidos;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];

    //Manda mensaje al entrenador
    sprintf(mensajeServer,"Bienvenido al Mapa!\n");//,argv[1]);
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);
    sprintf(mensajeServer,"Respondeme el saludo:\n");
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);

    //Recibe mensajes del entrenador, cuando recv devuelve 0 o negativo termina por desconexion o error
    while ((bRecibidos = recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0)) > 0 ) {
        printf("Cantidad de Bytes recibidos : %d\n", bRecibidos);
        mensajeCliente[bRecibidos] = '\0';
    	printf("Entrenador %d: %s\n", socketEntrenador, mensajeCliente);
    }

    if (bRecibidos == 0) {
        printf("Entrenador %d Desconectado!\n", socketEntrenador);
        fflush(stdout);
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
    }

    //Free the socket pointer
    free(socket);

    return 0;
}

/********************************************
 ****************** MAIN********************
 *******************************************/
int main(int argc , char *argv[]) {
	//system("clear");
	if (argc != 3) {
		puts("Cantidad de parametros incorrecto!");
		puts("Uso: mapa <nombre_mapa> <ruta PokeDex>");
		return EXIT_FAILURE;
	}

	//t_log *log = log_create(PATH_LOG_MAP, argv[1], true, 3);


	strcpy(argv[2],RUTA_POKEDEX);
	/* OBTENER METADATA DEL MAPA */
	tMapaMetadata *mapaMetadata = getMapaMetadata(argv[1],argv[2]);
	if (mapaMetadata == NULL) {
		puts("No se encontro el mapa.");
		return EXIT_FAILURE;
	}

	/* OBTENER METADATA DE LAS POKENESTS */
		tPokeNestMetadata *pokeNestMetadata = getPokeNestMetadata(argv[1], "Pikachu", argv[2]);
		if (pokeNestMetadata == NULL) {
			puts("No se encontro la PokeNest.");
			return EXIT_FAILURE;
		}

	/* OBTENER METADATA DE LOS POKEMON */
	tPokemonMetadata *pokemonMetadata = getPokemonMetadata(argv[1], "Pikachu", 1, argv[2]);
	if (pokemonMetadata == NULL) {
		puts("No se encontro el Pokemon.");
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

    addrlen = sizeof(struct sockaddr_in);

    while( (socketCliente = accept(socketEscucha, (struct sockaddr *)&cliente, (socklen_t*)&addrlen)) ) {
        puts("Conexión Aceptada!");

        pthread_t nuevoHilo;
        socketNuevo = malloc(1);
        *socketNuevo = socketCliente;

        if( pthread_create( &nuevoHilo, NULL,  gestor_de_entrenadores, (void*) socketNuevo) < 0 ) {
            perror("No se pudo crear el hilo para el Entrenador.");
            return EXIT_FAILURE;
        }

        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( nuevo_hilo , NULL);
        puts("Nuevo Hilo en Ejecucuión.");
    }

    if (socketCliente == -1 ) {
        perror("No se pudo Aceptar la conexión.");
        return EXIT_FAILURE;
    }

    free(mapaMetadata);
	//log_destroy(log);
    return EXIT_SUCCESS;
}
