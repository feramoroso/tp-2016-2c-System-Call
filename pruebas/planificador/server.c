
 /*
 ============================================================================
 Name        : serverMulti.c
 Description : Server Multi Conexiones
 ============================================================================
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#define MAXCLIENTES 10
#define MAXCHAR 50
#define MAXNOM 50
#define MIN 50
#define PORT 38432

void checkError(int, char*);

int main(void) {
	struct sockaddr_in direccionServer;
	struct sockaddr_in direccionCliente;
	int socketServer, socketCliente;
	int socketsDeClientes[MAXCLIENTES]; // Array que alberga los descriptores de los clientes que se nos conecten

	char buffer[MAXCHAR]; // Cadena para almacenar mensajes
	char hostname[MAXNOM]; // Cadena para almacenar el nombre del Server
	int yes = 1;

	int binder, reutilizer, listening, receive, sender; /*variables que toman los valores que devuelven las respectivas
														funciones para chequear errores*/
	int addrlen; // Cantidad de bytes de la Dirección
	int bytesRecibidos = 0, bufferSize = MIN;

	fd_set read_descriptors; /*Es una estructura que va a guardar los descriptores que esten para leer*/
	int maxDescriptores = 0; /*Indica la cantidad maxima de descriptores hasta el momento*/
	int posUltimoCliente = 0;
	int i = 0; // Para control ciclo for

	socketServer = socket(AF_INET, SOCK_STREAM, 0);
	checkError(socketServer, "SOCKET\n");

	reutilizer = setsockopt(socketServer, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
	checkError(reutilizer, "REUTILIZAR PUERTO\n");

	direccionServer.sin_family = AF_INET;
	direccionServer.sin_port = htons(PORT);
	direccionServer.sin_addr.s_addr = htonl(INADDR_ANY); // Mi propia dirección IP
	memset(&(direccionServer.sin_zero), '\0', 8); // Pongo en 0 el resto de la estructura

	gethostname(hostname, MAXNOM);
	printf("Host Name : %s\n",hostname);
	printf("Dirección : %s\n",inet_ntoa(direccionServer.sin_addr));
	printf("Puerto    : %d\n",PORT);

	binder = bind(socketServer, (struct sockaddr *)&direccionServer, sizeof(direccionServer));
	checkError(binder, "BIND\n");

	maxDescriptores = socketServer; // el socketServer va a ser el descriptor de valor maximo por el momento

	bzero(socketsDeClientes, MAXCLIENTES); // Pongo en 0 el Arreglo de Clientes

	/*pongo el socket a escuchar nuevas conexiones*/
	listening = listen(socketServer, MAXCLIENTES);
	checkError(listening, "LISTENING\n");
	puts("Escuchando...");

	/*meto el descriptor del socket del server en read_descriptores*/
	FD_ZERO(&read_descriptors); /*vacio read_descriptors primero*/
	FD_SET(socketServer, &read_descriptors); /*Meto el socket del servidor en la estructura*/

	while(1){

		select(maxDescriptores + 1, &read_descriptors, NULL, NULL, NULL); /*hago select*/

		/*se trata el server, se entra aca si se recibe una nueva conexion*/
		if (FD_ISSET(socketServer, &read_descriptors)){
			puts("AVISO: Nueva conexion.");

			addrlen = sizeof(struct sockaddr_in);
			socketCliente = accept(socketServer, (struct sockaddr *)&direccionCliente, &addrlen);
			checkError(socketCliente, "ACCEPT\n");
			puts("AVISO: Conexión Aceptada.");

			//agrego el descriptor al array de clientes y luego lo setteo en read_descriptors
			socketsDeClientes[posUltimoCliente] = socketCliente;
			FD_SET(socketCliente, &read_descriptors);
			puts("AVISO: Conexión Agregada al Arreglo de conexiones.");

			//asigno el valor del descriptor del cliente nuevo al de maxDescriptores si es mayor a este
			if (socketCliente>maxDescriptores) maxDescriptores = socketCliente;

			posUltimoCliente++;
		}


		/*se tratan los sockets clientes*/
		for (i = 0; i < MAXCLIENTES; i++) {
			if (FD_ISSET(socketsDeClientes[i], &read_descriptors)){
				/*uno de los clientes envio informacion, se tratan aca*/

				/*agrando el buffer hasta que sea lo suficientemente grande como para contener el mensaje en su totalidad*/
/*
				do{
					bufferSize += 10;
					if (buffer == NULL){
						buffer = malloc(MIN);
						bzero(buffer, MIN);
					} else {
						buffer = realloc(buffer, bufferSize);
						bzero(buffer, bufferSize);
					}

					bytesRecibidos = recv(socketsDeClientes[i], buffer, sizeof(buffer), MSG_PEEK);

				} while (bufferSize < bytesRecibidos);

				/*ahora si recibo el paquete completo*/


				receive = recv(socketsDeClientes[i], buffer, strlen(buffer), 0);
				checkError(receive, "RECV\n");
				printf("Cantidad de bytes recibidos : %d\n",receive);

				printf("Cliente %i: %s\n", i , buffer);

				//free(buffer);
				//buffer = NULL;

				sender = send(socketsDeClientes[i], "Mensaje enviado correctamente.", strlen("Mensaje enviado correctamente."), 0);
				checkError(sender, "SEND\n");
				printf("Cantidad de bytes enviados : %d\n",sender);
			}

		}

	}

	// Cierres
	for (i = 0; i <= posUltimoCliente; i++){
		close(socketsDeClientes[i]);
	}

    close(socketServer);
   // free(buffer);

	return EXIT_SUCCESS;
}

void checkError(int valor, char *mensajeError){
	if (valor == -1){
		perror(mensajeError);
		exit(1);
	}
}
