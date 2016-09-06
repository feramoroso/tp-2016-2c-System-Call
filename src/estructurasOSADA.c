/*
 * estructurasOSADA.c
 *
 *  Created on: 2/9/2016
 *      Author: JPfx
 */

#include <stdio.h>
#include <stdlib.h>

typedef struct {
	char id[7];
	char version;
	int tamFS;
	int tamBitmap;
	int inicioTabAsig;
	int tamDatos;
	char relleno[40];

} tHeader;

typedef struct {
	char estado;
	char nomArch[17];
	short int bloquePadre;
	int tamArch;
	int fecha;
	int bloqueIncial;
} osadaFile;

int main() {
	osadaFile archivo;
	printf("Introduzca el nombre del Archivo: ");
	scanf("%s",archivo.nomArch);
	printf("Usted ingreso:  %s\n",archivo.nomArch);
	printf("Tamaño Bloque Padre: %d\n",sizeof(archivo.bloquePadre));
	printf("Tamaño tHeader:      %d\n",sizeof(tHeader));
	printf("Tamaño osadaFile:    %d\n",sizeof(osadaFile));
	return 0;
}
