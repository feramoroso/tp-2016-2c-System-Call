#include<stdio.h>
#include<stdlib.h>
#include<stdint.h>
#include<string.h>
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
	
	fread(boot->buffer, OSADA_BLOCK_SIZE, 1, pokedex);
	printf("\nFileSystem Etiqueta: %s",boot->magic_number);
	printf("\nFileSystem Version: %d",boot->version );
	printf("\nFileSystem BLOCKS: %d",boot->fs_blocks);
	printf("\nFileSystem BitMap: %d",boot->bitmap_blocks);
	printf("\nFileSystem Offset Alloc: %d",boot->allocations_table_offset);
	printf("\nFileSystem Data Blocks: %d",boot->data_blocks);
	

	printf("\n\nOffset Bitmap:           0x%08X",(uint32_t) (1*OSADA_BLOCK_SIZE));
	printf(  "\nOffset Tabla Archivos:   0x%08X",(uint32_t) ((boot->bitmap_blocks+1)*OSADA_BLOCK_SIZE));
	printf(  "\nOffset Tabla Asignacion: 0x%08X", (uint32_t) ((boot->allocations_table_offset)*OSADA_BLOCK_SIZE));
	printf(  "\nOffset Datos:            0x%08X",(uint32_t) ((boot->allocations_table_offset + boot->fs_blocks - 1 - 1024 - boot->bitmap_blocks - boot->data_blocks )*OSADA_BLOCK_SIZE));


	printf("\nBitmap(primeros 7 bloques):\n");
	uint8_t byte[64];
	uint8_t mask;
	uint8_t *bitmap = calloc( boot->fs_blocks , sizeof(uint8_t));
	int i,j,k;
	fseek(pokedex, 1 * OSADA_BLOCK_SIZE, SEEK_SET );
	for(j=0 ; j<boot->bitmap_blocks ;j++){
		fread(byte, OSADA_BLOCK_SIZE, 1, pokedex);
		for(k=0 ; k<OSADA_BLOCK_SIZE ; k++){
			for (i=0 ; i<8 ; i++){
				mask = 1 << (7-i) ;
				bitmap[j*8*OSADA_BLOCK_SIZE   + k*8 + i] = byte[k] & mask ? 1 : 0 ;
			}
		}
	}
	for(i=0 ; i<boot->fs_blocks ; i++){
		if (i < 7 * OSADA_BLOCK_SIZE * 8){
			if(i%64==0)printf("\n");
			if(i%(64*8)==0)printf("Bloques %d a %d\n",i, i+64*8-1);
			if(i%8==0)printf(" ");
			if(i%32==0)printf(" ");
			printf("%d",bitmap[i]);
		}
	}
	free(bitmap);
	

	osada_file file_table[2048];
	fseek(pokedex,((boot->bitmap_blocks+1)*OSADA_BLOCK_SIZE), SEEK_SET );
	fread(file_table, sizeof(osada_file), 2048, pokedex);
	for(i=0;i<2048;i++){
		if (file_table[i].state != 0){
			printf("\nFile Status: %d",file_table[i].state);
			printf("\nFile Name: %s",file_table[i].fname);
			printf("\nFile Size: %d\n",file_table[i].file_size);
		}
	}

	//uint32_t asig_table[boot->data_blocks];
	uint32_t *asig_table = calloc(boot->data_blocks,sizeof(uint32_t));
	fseek(pokedex,((boot->allocations_table_offset)*OSADA_BLOCK_SIZE), SEEK_SET );
	fread(asig_table, sizeof(uint32_t), boot->data_blocks, pokedex);
	for(i=0;i<(boot->data_blocks);i++){
		if (asig_table[i] != 0xFFFFFFFF){
			//if (i%16==0) printf("\n");
			printf("Bloque[%d]: %08X\n",i,asig_table[i]);
		}
	}
	free(asig_table);

	
	
	printf("\nFin");
	free(boot);
	fclose(pokedex);
	return EXIT_SUCCESS;
}
