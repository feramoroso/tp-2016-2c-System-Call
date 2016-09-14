#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
#include<sys/mman.h>
#include "pokedex.h"

int main(){
	printf("PokeDex Servidor\n");
	
	FILE*	pokedex;
	osada_header	*boot = calloc(1,sizeof(osada_header));
	//char *buffer;
	if (NULL==(pokedex = fopen("pokedex2M","rb"))){
		printf("Error al cargar el disco Pokedex");
		return EXIT_FAILURE;
	}
	boot->buffer[0] ='\0';
	
	fgets(boot->buffer, 64,pokedex);
	printf("\nFileSystem Etiqueta: %s",boot->magic_number);
	printf("\nFileSystem Version: %d",boot->version );
	printf("\nFileSystem BLOCKS: %d",boot->fs_blocks);
	printf("\nFileSystem BitMap: %d",boot->bitmap_blocks);
	printf("\nFileSystem Offset Alloc: %d",boot->allocations_table_offset);
	printf("\nFileSystem Data Blocks: %d",boot->data_blocks);
	
	printf("\n\nBitmap:\n");
	uint8_t byte[64];
	uint8_t mask;
	int i,j,k;
	fseek(pokedex,64, SEEK_SET );
	for(j=0;j<(boot->bitmap_blocks);j++){
		printf("Bloque: %d\n",j+1);
		fgets(byte, 64,pokedex);
		for(k=0;k<64;k++){

			for (i=0 ; i<8 ; i++){
				mask = 1 << (7-i) ;
				printf(byte[k] & mask ? "1" : "0");
			}
			printf(" ");
			if ((k+1)%4 == 0) printf(" ");
			if ((k+1)%8 == 0) printf("\n");
		}

	}
	

	osada_file *file=calloc(1,sizeof(osada_file));
	fseek(pokedex,((boot->bitmap_blocks)*64), SEEK_SET );
	for(i=0;i<2048;i++){
		fgets(file->buffer, 32,pokedex);
		if (file->state != 0){
			printf("\nFile Status: %d",file->state);
			printf("\nFile Status: %s",file->fname);
			printf("\nFile Status: %d",file->file_size);
		}
	}
	free(file);

	printf("\nFin");
	
	

	free(boot);
	fclose(pokedex);
	
	return EXIT_SUCCESS;
}
