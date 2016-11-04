#ifndef __POKE_H
#define __POKE_H

#include <stdint.h>
#include <semaphore.h>
#include "socks_fs.h"

#define OSADA_BLOCK_SIZE 64
#define OSADA_FILENAME_LENGTH 17

typedef unsigned char osada_block[OSADA_BLOCK_SIZE];

typedef enum __attribute__((packed)) {
    DELETED = '\0',
    REGULAR = '\1',
    DIRECTORY = '\2',
} osada_file_state;

typedef struct {
	t_log			*log;
	osada_socket	sock;
	sem_t			mux_socket;
}fs_osada_t;

#endif
