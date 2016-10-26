#ifndef __OSAD_H
#define __OSAD_H

#include <stdint.h>

#define MAX_CONECTIONS 20

enum {
    OK           = 0,
    OP_GETATTR   = 1,	// static int osada_getattr(const char *path, struct stat *stbuf)
							//		IN:	 path		OUT: COD, size, lastmod, REGULAR o DIRECTORY
    OP_READDIR   = 2,	//static int osada_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
							//		IN:	 path		OUT: COD, Lista{size, lastmod, REGULAR o DIRECTORY}
    OP_OPEN      = 3,	//static int osada_open(const char *path, struct fuse_file_info *fi)
							//		IN:	 path		OUT: COD
	OP_CREATE    = 4,	//int osada_create (const char *path, mode_t mode, struct fuse_file_info *fi)
							//		IN:	 path		OUT: COD
	OP_READ      = 5,	//static int osada_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
							//		IN:	 path, size, offset, fi		OUT: COD or copied, buf
	OP_TRUNCATE  = 6,	//int osada_truncate(const char * path, off_t offset)
							//		IN:	 path, offset		OUT: COD
	OP_WRITE     = 7,	//int osada_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
							//		IN:	 path, buf, size, offset, fi		OUT: COD or copied
	OP_UNLINK    = 8,	//int osada_unlink (const char *path)
							//		IN:	 path		OUT: COD
	OP_RMDIR     = 9,	//int osada_rmdir (const char *path)
							//		IN:	 path		OUT: COD
	OP_FTRUNCATE = 10,	//int osada_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
							//		IN:	 path, offset		OUT: COD
	OP_MKDIR     = 11,	//int osada_mkdir (const char *path, mode_t mode)
							//		IN:	 path		OUT: COD
	OP_RENAME    = 12,	//int osada_rename (const char *from, const char *to)
							//		IN:	 from, to		OUT: COD
	OP_STATFS    = 13,	//int osada_statfs(const char *path, struct statvfs *statvfs)
							//		IN:	 path		OUT: COD, FS_Blocks, FREE_Blocks
	OP_UTIMENS   = 14,	//int osada_utimens(const char* path, const struct timespec ts[2])
							//		IN:	 path, lastmod		OUT: COD
}; // Tipo de Mensaje u Operacion

/* Opcion 1 - Datos Variables
typedef struct osada_packet {
        uint8_t    type;	// Nro de Operacion o Codigo de error ERRNO
        uint16_t   len;		// Tamaño del campo data
        uint8_t    *data;	// Datos de de cada Operacion
    }osada_packet;
*/
// Opcion 2 - Estructura fija
typedef union osada_packet {
    uint8_t buffer[549];
    struct {
        uint8_t    type;
        uint16_t   len;
		int32_t    cod_return;
        uint8_t    file_state;
		uint32_t   size;
		uint32_t   offset;
		uint32_t   lastmod;
		uint8_t    fname[17];
        uint8_t    path[256];
		uint8_t    pathto[256];
    } __attribute__ ((packed));
} osada_packet;

typedef int32_t osada_socket;

osada_socket    create_socket();
int8_t          bind_socket(osada_socket socket, char *host, uint16_t port);
int8_t          connect_socket(osada_socket socket, char *host, uint16_t port);
int32_t         send_socket(osada_packet *packet, osada_socket socket);
int32_t         recv_socket(osada_packet *packet, osada_socket socket);
void            listen_socket(osada_socket sock);
void            close_socket(osada_socket sock);

#endif
