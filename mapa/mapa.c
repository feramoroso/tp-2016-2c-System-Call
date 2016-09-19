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
#include <commons/error.h>
#include <commons/config.h>
#include <commons/string.h>
#include <commons/log.h>
#include <commons/collections/list.h>
#include <commons/collections/queue.h>

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
tPokeNestMetadata *pokeNestArray[10];  // Arreglo con las distintas PokeNest
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
    int x = 1, y = 1;
    char mensajeServer[TAM_MENSAJE] , mensajeCliente[TAM_MENSAJE];

    //Manda mensaje al entrenador
    sprintf(mensajeServer,"\nBienvenido al Mapa %s!\n\nEmpieza el juego!\n\nIngrese Simbolo Personaje:", mapaMetadata->nombre);
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);

    bRecibidos = recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0);
    if (bRecibidos == 0) {
        printf("Entrenador %d Desconectado!\n", socketEntrenador);
        fflush(stdout);
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
    }

    simbolo = mensajeCliente[0];
    CrearPersonaje(items, simbolo, x, y);
    nivel_gui_dibujar(items, mapaMetadata->nombre);

    sprintf(mensajeServer,"Buena suerte!\n\n1 - Derecha\n2 - Izquierda\n3 - Arriba\n4 - Abajo\n");
    send(socketEntrenador, mensajeServer, strlen(mensajeServer), 0);

    while ((bRecibidos = recv(socketEntrenador, mensajeCliente, TAM_MENSAJE, 0)) > 0 ) {
    	switch (mensajeCliente[0]) {
    	case '1':
    		x++;
    		break;
    	case '2':
    		x--;
    		break;
    	case '3':
    		y--;
    		break;
    	case '4':
    		y++;
    		break;
    	}
    	MoverPersonaje(items, simbolo, x, y);
    	nivel_gui_dibujar(items, mapaMetadata->nombre);
    }
    if (bRecibidos == 0) {
        printf("Entrenador %c Desconectado!", simbolo);
        fflush(stdout);
    }
    else if(bRecibidos == -1) {
        perror("Error de Recepción");
    }

    BorrarItem(items, simbolo);
    nivel_gui_dibujar(items, mapaMetadata->nombre);
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
			i++;
		}
	}
	pokeNestArray[i] = NULL;
	closedir(dir);
	return EXIT_SUCCESS;
}

int getPokemonsQueue(char *nomMapa, char *rutaPokeDex) {
	int i = 0, j;
	tPokemonMetadata *pokemonMetadata;
	while (pokeNestArray[i]) {
		pokeNestArray[i]->pokemons = queue_create();
		char ruta[256];
		sprintf(ruta, "%s/Mapas/%s/PokeNests/%s", rutaPokeDex, nomMapa, pokeNestArray[i]->nombre);
		j = 1;
		while ((pokemonMetadata = getPokemonMetadata(pokeNestArray[i]->nombre, j, ruta))) {
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
			printf("\nInstancias:       %d", queue_size(pokeNestArray[i]->pokemons));
			printf("\nNivel 1º en cola: %d\n\n\n", (*(tPokemonMetadata*)(queue_peek(pokeNestArray[i]->pokemons))).nivel);
		}
		i++;
	}
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
	/* OBTENER ARREGLO DE POKENEST */
	if (getPokeNestArray(mapaMetadata->nombre, argv[2])) {
		error_show("PokeNest invalida.\n");
		return EXIT_FAILURE;
	}

	/* OBTENER COLA DE POKEMONS EN CADA POKENEST */
	if (getPokemonsQueue(mapaMetadata->nombre, argv[2])) {
		error_show("Error en la PokeNest.\n");
		return EXIT_FAILURE;
	}
	imprimirInfoPokeNest();


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
    puts("Presione Enter para continuar...");
    getchar();



    /*****************************
    ******** MODO GRAFICO ********
    *****************************/
    items = list_create();
    int i = 0;
    while (pokeNestArray[i]) {
    	CrearCaja(items, pokeNestArray[i]->id, pokeNestArray[i]->posx, pokeNestArray[i]->posy, queue_size(pokeNestArray[i]->pokemons));
    	i++;
    }

    nivel_gui_inicializar();
    nivel_gui_dibujar(items, mapaMetadata->nombre);
    /*****************************/

    addrlen = sizeof(struct sockaddr_in);
    while( (socketCliente = accept(socketEscucha, (struct sockaddr *)&cliente, (socklen_t*)&addrlen)) ) {
        //puts("Conexión Aceptada!");

        pthread_t nuevoHilo;
        socketNuevo = malloc(1);
        *socketNuevo = socketCliente;

        if( pthread_create( &nuevoHilo, NULL,  gestor_de_entrenadores, (void*) socketNuevo) ) {
            perror("No se pudo crear el hilo para el Entrenador.");
            return EXIT_FAILURE;
        }
        //puts("Nuevo Hilo en Ejecución.");
        //Por el momento no vamos a esperar que termine
        //pthread_join( nuevo_hilo , NULL);
    }
    nivel_gui_terminar();
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
