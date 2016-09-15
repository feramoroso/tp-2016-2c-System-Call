/*
 * pokeServ.h
 *
 *  Created on: 15/9/2016
 *      Author: utnso
 */

#ifndef POKESERV_H_
#define POKESERV_H_

#include <stdint.h>
#include <commons/log.h>

#define OSADA_BLOCK_SIZE 64
#define OSADA_FILENAME_LENGTH 17
#define MAX_FILES 10

typedef unsigned char osada_block[OSADA_BLOCK_SIZE];
typedef uint32_t osada_block_pointer;

// set __attribute__((packed)) for this whole section
// See http://stackoverflow.com/a/11772340/641451
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

//_Static_assert( sizeof(osada_header) == sizeof(osada_block), "osada_header size does not match osada_block size");

typedef enum __attribute__((packed)) {
    DELETED = '\0',
    REGULAR = '\1',
    DIRECTORY = '\2',
} osada_file_state;

//_Static_assert( sizeof(osada_file_state) == 1, "osada_file_state is not a char type");

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

//_Static_assert( sizeof(osada_file) == (sizeof(osada_block) / 2.0), "osada_file size does not half osada_block size");

typedef struct {
	osada_header	header;
	osada_file 		file_table[MAX_FILES];
	t_log			*log;
}fs_osada_t;


#pragma pack(pop)


#endif /* POKESERV_H_ */
