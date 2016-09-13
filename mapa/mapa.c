/*
    PROCESO MAPA
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h> //for threading , link with lpthread

#define MAX_CON 10     // Conexiones máximas
#define PUERTO 6666    // Puerto de Escucha
#define T_NOM_HOST 128 // Tamaño del nombre del Server
#define T_MENSAJE 128  // Tamaño del mensaje del cliente

/*
 * FUNCION DE HILO
 * Maneja las conexiones para cada cliente
 * */
void *connection_handler(void *socketEntrenador)
{
    //Castea el descriptor del socket
    int sock = *(int*)socketEntrenador;
    int bRecibidos;
    char *message , client_message[T_MENSAJE];

    //Manda mensaje al entrenador
    message = "Bienvenido al Mapa <mapa>!\n";
    write(sock , message , strlen(message));

    message = "Respondeme el saludo: ";
    write(sock , message , strlen(message));

    //Recibe mensajes del entrenador cuando recv devuelve 0 o negeativo termina por desconexion o error
    while( (bRecibidos = recv(sock , client_message , T_MENSAJE , 0)) > 0 )
    {
            	printf("Cantidad de Bytes recibidos : %d\n", bRecibidos);
    	client_message[bRecibidos] = '\0';
    	printf("Entrenador %d: %s\n", sock, client_message);
    	//write(sock , client_message , strlen(client_message));
    }

    if(bRecibidos == 0)
    {
        printf("Entrenador %d Desconectado!\n", sock);
        fflush(stdout);
    }
    else if(bRecibidos == -1)
    {
        perror("Error de Recepción");
    }

    //Free the socket pointer
    free(socketEntrenador);

    return 0;
}

/********************************************
 ****************** MAIN********************
 *******************************************/
int main(int argc , char *argv[])
{
	if (argc != 2) {
		system("clear");
		printf("Cantidad de parametros incorrecto!\n");
		printf("Uso ./mapa <nombre_mapa> <ruta>\n");
		return (1);
	}

	int socket_desc , client_sock , c , *new_sock;
    struct sockaddr_in server , client;

	char hostname[T_NOM_HOST]; // Cadena para almacenar el nombre del Server

    //Creacion del Socket TCP
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        puts("No se pudo crear el socket.");
        return 1;
    }
    puts("Socket creado!\n");

    //Preparando la estructura
    //server.sin_family = AF_INET;
    //server.sin_addr.s_addr = INADDR_ANY;
    //server.sin_port = htons( 8888 );
	server.sin_family = AF_INET;
	server.sin_port = htons(PUERTO);
	server.sin_addr.s_addr = htonl(INADDR_ANY); // Mi propia dirección IP
	memset(&(server.sin_zero), '\0', 8); // Pongo en 0 el resto de la estructura

	gethostname(hostname, T_NOM_HOST);
	printf("Host Name : %s\n",hostname);
	printf("Dirección : %s\n",inet_ntoa(server.sin_addr));
	printf("Puerto    : %d\n\n",PUERTO);

	//Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("bind failed. Error");
        return 1;
    }
    puts("Bind exitoso!");

    //Listen
    listen(socket_desc , MAX_CON);

    //Accept and incoming connection
    puts("Waiting for incoming connections...");
    c = sizeof(struct sockaddr_in);

    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        puts("Conexión Aceptada!");

        pthread_t sniffer_thread;
        new_sock = malloc(1);
        *new_sock = client_sock;

        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("No se pudo crear el hilo para el Entrenador.");
            return 1;
        }

        //Now join the thread , so that we dont terminate before the thread
        //pthread_join( sniffer_thread , NULL);
        puts("Handler assigned");
    }

    if (client_sock == -1 ) {
        perror("Error de accept");
        return 3;
    }

    return 0;
}
