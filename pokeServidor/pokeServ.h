#ifndef POKESERV_H_
#define POKESERV_H_

#include <stdint.h>
#include <commons/log.h>
#include <semaphore.h>
#include <pthread.h>
#include "socks_fs.h"

#define OSADA_BLOCK_SIZE 64
#define OSADA_FILENAME_LENGTH 17
#define MAX_FILES 2048

typedef unsigned char osada_block[OSADA_BLOCK_SIZE];
typedef uint32_t osada_block_pointer;

#pragma pack(push, 1)

typedef union osada_header {
    uint8_t buffer[64];              // Offset   Longitud
    struct {
		unsigned char magic_number[7]; // OSADAFS
		uint8_t version;
		uint32_t fs_blocks; // total amount of blocks
		uint32_t bitmap_blocks; // bitmap size in blocks
		uint32_t allocations_table_offset; // allocations table's first block number
		uint32_t data_blocks; // amount of data blocks
		unsigned char padding[40]; // useless bytes just to complete the block size
	};
} osada_header;

typedef enum __attribute__((packed)) {
    DELETED = '\0',
    REGULAR = '\1',
    DIRECTORY = '\2',
} osada_file_state;

typedef union osada_file {
    uint8_t buffer[32];              // Offset   Longitud
    struct {
	osada_file_state state;
	unsigned char fname[OSADA_FILENAME_LENGTH];
	uint16_t parent_directory;
	uint32_t file_size;
	uint32_t lastmod;
	osada_block_pointer first_block;
	};
} osada_file;

typedef struct {
	osada_header	header;
	uint8_t			*bitmap;
	osada_file 		file_table[MAX_FILES];
	uint32_t		*fat_osada;
	t_log			*log;
	sem_t			mux_osada;
	pthread_rwlock_t	mux_files[MAX_FILES];
}fs_osada_t;

typedef struct _lista_pokeCli{
	osada_socket sock;
	sem_t semaforo;
	struct _lista_pokeCli *sgte;
} __attribute__ ((packed)) lista_pokeCli;

#pragma pack(pop)

int is_parent(osada_file table[], int8_t *path);
int32_t free_bit_bitmap(uint8_t *bitmap);
//uint32_t free_blocks(uint8_t *bitmap);
uint32_t bit_bitmap(uint8_t *bitmap, uint32_t pos);
int set_bitmap(uint8_t *bitmap, uint32_t pos);
int clean_bitmap(uint8_t *bitmap, uint32_t pos);
void liberar_cliente_caido(osada_socket sock);

#endif
