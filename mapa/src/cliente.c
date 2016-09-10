
 /*
 ============================================================================
 Name        : client.c
 Description : CLIENT
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
#include <netdb.h>

#define BACKLOG 10
#define MAXCHAR 10
#define MIN 50
#define PORT 38432

/*
 * Estructura sockaddr_in
 *
 * Reemplaza a sockaddr, generalmente se lo usa casteado: (struct sockaddr *)VariableDeTipoSockaddr_in
 *
 * Nombre de Valor: descripcion: que se le asigna
 * sin_family: familia de direcciones: AF_INET
 * sin_port: numero de puerto: htons(NUMPUERTO)
 * sin_addr.s_addr: direccion de internet: inet_addr("direccion IP") o htonl(INADDR_ANY) si uso la de mi PC
 * sin_zero[8]: relleno para preservar el tamanho: inicializar con memset(puntero, valor, bytes)
 *
 */

void checkError(int, char*);

int main(void) {
	int sockfd, s, r;
	char buffer[MAXCHAR];
	char hostname[40];
	struct hostent *server;
	struct sockaddr_in direccionServer;
	int bytesRecibidos = 0, bufferSize = MIN;
	char mensaje[300];

	/*obtengo el hostname*/

	gethostname(hostname, sizeof hostname);
	printf("El nombre del Host es : %s\n",hostname);

	/*setteo descriptor del socket al que me voy a conectar*/
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	checkError(sockfd, "SOCKET\n");

	server = gethostbyname(hostname);

	if (server == NULL) {
	   perror("gethostbyname");
	   exit(1);
	}

	/*setteo la direccion del server*/

	direccionServer.sin_family = AF_INET;
	direccionServer.sin_port = htons(PORT);
    bcopy((char *)server->h_addr,(char *)&direccionServer.sin_addr.s_addr,server->h_length);
	memset(&(direccionServer.sin_zero), '\0', 8);

	/*conecto a server*/
	checkError(connect(sockfd, (struct sockaddr *)&direccionServer, sizeof(direccionServer)), "CONNECT\n");

	/*mando un mensaje*/

	puts("Manda un mensaje:");
	scanf("%s", mensaje);

	s = send(sockfd, mensaje, strlen(mensaje), 0);
	printf("Cantidad de bytes enviados : %d\n",s);
	checkError(s, "SEND\n");

	/*agrando el buffer hasta que sea lo suficientemente grande como para contener todo el paquete
	do{
		bufferSize += 10;
		if (buffer == NULL){
			buffer = malloc(MIN);
			bzero(buffer, MIN);
		} else {
			buffer = realloc(buffer, bufferSize);
			//bzero(buffer, bufferSize);
		}

		bytesRecibidos = recv(sockfd, buffer, sizeof(buffer), MSG_PEEK);

	} while (bufferSize < bytesRecibidos);

	/*ahora si recibo el mensaje*/
	r = recv(sockfd, buffer, strlen(buffer), 0);
	printf("Cantidad de bytes recibidos : %d\n",r);
	checkError(r, "RECV\n");

	printf("Server: %s\n", buffer);

	/*cierres*/
	//free(buffer);
	close(sockfd);

	return EXIT_SUCCESS;
}

void checkError(int valor, char *mensajeError){
	if (valor == -1){
		perror(mensajeError);
		exit(1);
	}
}
