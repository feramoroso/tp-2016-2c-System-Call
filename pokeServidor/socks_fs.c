#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "socks_fs.h"

/**
 * Inicia un socket para escuchar nuevas conexiones
 *
 * @osada_socket descriptor del socket
 * @return el descriptor del socket creado
 */
osada_socket create_socket()
{
	osada_socket sock_new;
	int i = 1;

	if ((sock_new = socket(AF_INET,SOCK_STREAM,0))<0)
	{
		printf("Error socket");
		exit(EXIT_FAILURE);
	}
	if (setsockopt(sock_new, SOL_SOCKET, SO_REUSEADDR, &i,sizeof(sock_new)) < 0)
	{
		printf("\nError: El socket ya esta siendo utilizado...\n");
		exit(EXIT_FAILURE);
	}

    return sock_new;
}

int8_t bind_socket(osada_socket socket, char *host, uint16_t port)
{
	struct sockaddr_in addr_raid;

	addr_raid.sin_family = AF_INET;
	addr_raid.sin_port=htons(port);
	addr_raid.sin_addr.s_addr=inet_addr(host);
	if (bind(socket,(struct sockaddr *)&addr_raid,sizeof(struct sockaddr_in))<0)
	{
		printf("Error bind\n");
		return -EADDRNOTAVAIL;
	}

	return 0;
}

int8_t connect_socket(osada_socket socket, char *host, uint16_t port)
{
	struct sockaddr_in addr_raid;

	memset(&addr_raid, '\0', sizeof(struct sockaddr_in));
	addr_raid.sin_family = AF_INET;
	addr_raid.sin_port=htons(port);
	addr_raid.sin_addr.s_addr=inet_addr(host);
	if (connect(socket,(struct sockaddr *)&addr_raid,sizeof(struct sockaddr_in))<0)
	{
		printf("Error connect %d %s(%d) %d %d(%s)\n",socket, host, strlen(host), port, errno, strerror(errno));
		return -EADDRNOTAVAIL;
	}
	return 0;
}

/**
 * Recibe un paquete del socket indicado
 *
 * @return cantidad de caracteres recividos, en caso de ser 0 informa que el socket ha sido cerrado
 */
int32_t recv_socket(osada_packet *packet, osada_socket sock)
{
	int32_t   leido = 0;
	int32_t   aux = 0;

	//* Comprobacion de que los parametros de entrada son correctos
	if ((sock == -1) || (packet == NULL))
		return -1;
	//* Mientras no hayamos leido todos los datos solicitados
	aux = recv (sock, packet, 3, 0);
	leido = leido + aux;
	while (leido < (3 + packet->len))
	{
		aux = recv(sock, packet->buffer + leido, (3 + packet->len) - leido,0);
		//printf("Recibido: %d bytes.\n", aux);
		if (aux > 0)
		{
			//* Si hemos conseguido leer datos, incrementamos la variable
			//* que contiene los datos leidos hasta el momento
			leido = leido + aux;
		}
		else
		{
			//* Si read devuelve 0, es que se ha cerrado el socket. Devolvemos
			//* los caracteres leidos hasta ese momento
			if (aux == 0)
			{
				return leido;
			}
			if (aux == -1)
			{  printf("\n\n---- Error Socks_fs - RECV: %s\n\n",strerror(errno));fflush(stdout);
				/*
				* En caso de error, la variable errno nos indica el tipo
				* de error.
				* El error EINTR se produce si ha habido alguna
				* interrupcion del sistema antes de leer ningun dato. No
				* es un error realmente.
				* El error EGAIN significa que el socket no esta disponible
				* de momento, que lo intentemos dentro de un rato.
				* Ambos errores se tratan con una espera de 100 microsegundos
				* y se vuelve a intentar.
				* El resto de los posibles errores provocan que salgamos de
				* la funcion con error.
				*/
				switch (errno)
				{
					case EINTR:
					case EAGAIN:
						usleep(100);
						break;
					default:
						return -1;
				}
			}
		}
	}
	return leido;
}

/**
 * Envia un paquete en el socket especificado
 *
 * @return cantidad de caracteres enviados
 */
int32_t send_socket(osada_packet *packet, osada_socket sock)
{
	int32_t Escrito = 0;
	int32_t Aux = 0;
	//* Comprobacion de los parametros de entrada
	if ((sock == -1) || (packet == NULL) || (519 < 1))
		return -1;
    /*
    * Preparo el paquete para ser enviado
    */

    //ídem del recv. si le damos el payload al recv como buffer no vamos a
    //tener que manejar todos los casos posibles para el paquete

    //* Bucle hasta que hayamos escrito todos los caracteres que nos han
    //* indicado.
    while (Escrito < (3 + packet->len))
    {
		Aux = send(sock, packet + Escrito, (3 + packet->len) - Escrito,0);
		if (Aux > 0)
		{
			//* Si hemos conseguido escribir caracteres, se actualiza la
			//* variable escrito
			Escrito = Escrito + Aux;
		}
		else
		{  printf("\n\n---- Error Socks_fs - SEND: %s\n\n",strerror(errno));fflush(stdout);
			//* Si se ha cerrado el socket, devolvemos el numero de caracteres
			//* leidos.
			//* Si ha habido error, devolvemos -1
			if (Aux == 0)
				return Escrito;
			else
				return -1;
		}
    }
    //printf("\n---- SEND type(%d): %d de %d\n", packet->type, Escrito, packet->len);fflush(stdout);
    return Escrito;
}

/**
 * Pone en escucha al socket especificado
 *
 */
void listen_socket(osada_socket sock)
{
	if ((listen(sock,MAX_CONECTIONS))<0)
	{
		printf("Error listen");
		exit(EXIT_FAILURE);
	}
}

/**
 * Cierra el socket especificado
 *
 */
void close_socket(osada_socket sock)
{
	if (close(sock)<0)
	{
		printf("Error close");
		exit(EXIT_FAILURE);
	}
}
