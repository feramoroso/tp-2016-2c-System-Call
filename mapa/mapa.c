#include <stdio.h>
#include <string.h>    //strlen
#include <stdlib.h>    //strlen
#include <sys/socket.h>
#include <arpa/inet.h> //inet_addr
#include <unistd.h>    //write
#include <pthread.h> //for threading , link with lpthread

int main(int argc , char *argv[]) {
	if (argc != 2) {
		printf("Cantidad de parametros incorrecto!\n");
		printf("Uso ./mapa <nombre_mapa> <ruta>\n");
		return (1);
	}



}
