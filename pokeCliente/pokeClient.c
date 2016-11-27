#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fuse.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include "pokeClient.h"
#include "socks_fs.h"

#define FILE_LOG "logFUSE.txt"

/*
 * osada_read: Funcion FUSE
 */
static int osada_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA read: %s-%d-%d", path, (int)offset, (int)size);
	printf("\nREAD: %s-Off:%d-Size:%d - ", path, (int)offset, (int)size);fflush(stdout);
	
	osada_packet mensaje;
	mensaje.type = OP_READ;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	mensaje.size = size;
	mensaje.offset = offset;
	//mensaje.lastmod = fi->fh;
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}
	/*
	 * Espero respuesta del Servidor
	 */
	int32_t readed=0;
	mensaje.size = 1;
	while ( mensaje.size == 1 ){
		cant = recv_socket(&mensaje,fs_tmp->sock);
		if(cant <= 0){
			sem_post(&fs_tmp->mux_socket);
			log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
			return -EFAULT;
		}
		if (mensaje.type != OP_READ){
			printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_READ);fflush(stdout);
		}
		if (mensaje.cod_return < 0){
			sem_post(&fs_tmp->mux_socket);
			log_error(fs_tmp->log, "    %s", strerror(mensaje.cod_return));
			return mensaje.cod_return;
		}
		memcpy(buf + readed, mensaje.path, mensaje.cod_return);
		readed += mensaje.cod_return;
		//log_trace(fs_tmp->log, "mensaje: %s",mensaje.path);
	}
	sem_post(&fs_tmp->mux_socket);
	printf("Fin READ\n");fflush(stdout);
	return readed;
}

static int osada_getattr(const char *path, struct stat *stbuf)
{
	printf("\nGETATTR: %s ", path);fflush(stdout);
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	//log_info(fs_tmp->log, "OSADA getattr: %s", path);
	memset(stbuf, 0, sizeof(struct stat));
	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje
	if(memcmp(path, "/", 2) == 0) {
	        stbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	        stbuf->st_nlink = 2;
	        stbuf->st_uid = context->uid;
	        stbuf->st_gid = context->gid;
	        return 0;
	}
	printf("No raiz - ");fflush(stdout);
	osada_packet mensaje;
	mensaje.type = OP_GETATTR;
	mensaje.len = 290;//sizeof(osada_packet)-3;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&(fs_tmp->mux_socket));
	printf("Send - ");fflush(stdout);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}
	printf("Recv - ");fflush(stdout);
	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	if (mensaje.type != OP_GETATTR){
		printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_GETATTR);fflush(stdout);
	}
	printf("Recv OK - ");fflush(stdout);
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0){
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
		return mensaje.cod_return;
	}

	stbuf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	stbuf->st_nlink = 1;
	stbuf->st_uid = context->uid;
	stbuf->st_gid = context->gid;
	stbuf->st_size = mensaje.size;
	stbuf->st_blksize = OSADA_BLOCK_SIZE;
	stbuf->st_blocks = (mensaje.size / OSADA_BLOCK_SIZE) + 1;
	stbuf->st_mtim.tv_sec = (time_t)mensaje.lastmod;

	if (mensaje.file_state == DIRECTORY)
		stbuf->st_mode |= S_IFDIR;
	else
		stbuf->st_mode |= S_IFREG;
	printf("Fin GETATTR");fflush(stdout);
	return mensaje.cod_return;
}

static int osada_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	printf("\nREADDIR: %s ", path);fflush(stdout);
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;

	log_info(fs_tmp->log, "OSADA readdir: %s", path);
	osada_packet mensaje;
	mensaje.type = OP_READDIR;
	mensaje.len = 290; //sizeof(osada_packet)-3;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	printf("Send - ");fflush(stdout);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}
printf("Recv - ");fflush(stdout);
	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
printf("Recv OK(%d) - ",cant);
//printf("\n    Type:%d - Len:%d - Return:%d - State:%d - size:%d - Off:%d - lastmod:%d - Name:%s\n", mensaje.type, mensaje.len, mensaje.file_state, mensaje.size, mensaje.offset, mensaje.lastmod, mensaje.fname);fflush(stdout);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	if (mensaje.type != OP_READDIR){
		printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_READDIR);fflush(stdout);sleep(5);
	}
	//log_trace(fs_tmp->log, "    Cant: %d", mensaje.offset );
	if (mensaje.cod_return < 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
		return mensaje.cod_return;
	}

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	int32_t i=0,cantFiles=0;
	if (mensaje.offset == 0 ){
		sem_post(&fs_tmp->mux_socket);
		return 0;
	}
	cantFiles = mensaje.offset;
	while (i < cantFiles){
		struct stat var_stat = {
			.st_mode = S_IRWXU | S_IRWXG | S_IRWXO,
			.st_nlink = 1,
			.st_uid = context->uid,
			.st_gid = context->gid,
			.st_size = mensaje.size,
			.st_blksize = OSADA_BLOCK_SIZE,
			.st_blocks = (mensaje.size / OSADA_BLOCK_SIZE) + 1
		};
		if (mensaje.file_state == DIRECTORY)
			var_stat.st_mode |= S_IFDIR;
		else
			var_stat.st_mode |= S_IFREG;
		//log_info(fs_tmp->log, "    %d - File(%d): %s", i, strlen(mensaje.fname), mensaje.fname);
		mensaje.fname[17]='\0';
		filler(buf, mensaje.fname, &var_stat, 0);
		i++;
		if (i < cantFiles){
			printf("    Recv ");fflush(stdout);
			cant = recv_socket(&mensaje,fs_tmp->sock);
			printf("OK(%d) - ",cant);fflush(stdout);
			//printf("\n    Type:%d - Len:%d - Return:%d - State:%d - size:%d - Off:%d - lastmod:%d - Name:%s\n", mensaje.type, mensaje.len, mensaje.file_state, mensaje.size, mensaje.offset, mensaje.lastmod, mensaje.fname);fflush(stdout);
			if(cant <= 0){
				sem_post(&fs_tmp->mux_socket);
				log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
				return -EFAULT;
			}
			if (mensaje.type != OP_READDIR){
				printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_READDIR);fflush(stdout);sleep(5);
			}
		}
	}
	sem_post(&fs_tmp->mux_socket);
	printf("    Fin READDIR");fflush(stdout);
	return mensaje.cod_return;
}

int osada_mkdir (const char *path, mode_t mode)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA mkdir: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_MKDIR;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

int osada_rmdir (const char *path)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA rmdir: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_RMDIR;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

int osada_rename (const char *from, const char *to)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA rename: %s - %s", from, to);

	osada_packet mensaje;
	mensaje.type = OP_RENAME;
	mensaje.len = 546;
	strcpy((char *)mensaje.path, from);
	strcpy((char *)mensaje.pathto, to);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

static int osada_open(const char *path, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA open: %s - %X", path, fi->flags);
	printf("\nOPEN: %s - %X", path, fi->flags);fflush(stdout);

	osada_packet mensaje;
	mensaje.type = OP_OPEN;
	mensaje.len = 290;

	if((fi->flags & O_TRUNC) && ((fi->flags & O_RDWR) || (fi->flags & O_WRONLY)))
		mensaje.offset = 2;
	else if((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))
		mensaje.offset = 1;
	else
		mensaje.offset = 0;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	/*else{
		//fi->fh = mensaje.offset;
		//log_trace(fs_tmp->log, "   Open OK: %d", mensaje.cod_return);
		if((fi->flags & O_TRUNC) && ((fi->flags & O_RDWR) || (fi->flags & O_WRONLY))) {
			//log_info(fs_tmp->log, "Open: truncando a 0");
			return osada_ftruncate(path, 0, NULL);
		}
	}*/
	printf("    Fin OPEN");fflush(stdout);
	return mensaje.cod_return;
}


int osada_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA create: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_CREATE;
	mensaje.len = 290;
	//mensaje.offset = fi->fh;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}//log_info(fs_tmp->log, "    Enviado %d",cant);
	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv(%d): %s", cant, strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	osada_open(path, fi);
	return mensaje.cod_return;
}

int osada_statfs(const char *path, struct statvfs *statvfs)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log,"OSADA statfs: %s", path);
	printf("STATFS: %s\n", path);
	osada_packet mensaje;
	mensaje.type = OP_STATFS;
	mensaje.len = 0;
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	else{
		statvfs->f_bsize = OSADA_BLOCK_SIZE;
		statvfs->f_blocks = mensaje.size;
		statvfs->f_bavail = statvfs->f_bfree = mensaje.offset;
	}
	return mensaje.cod_return;
}

int osada_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA write: %s - Size:%d - Offset: %d", path, (int)size, (int)offset);

	uint32_t partOffset, partSize, writed;
	int32_t cant;
	osada_packet mensaje;
	partOffset = offset;
	partSize = 0;
	writed = 0;

	sem_wait(&fs_tmp->mux_socket);
	do{
		mensaje.type = OP_WRITE;
		mensaje.len = 546;
		strcpy(mensaje.path, path);
		if ((size-writed) < 256){
			partSize = (size-writed);
			//log_trace(fs_tmp->log, "   Sale Diferencia: size-writed: %d", (size-writed));
			//mensaje.cod_return = 0; //ultimo
		}
		else{
			partSize = 256;
			//log_trace(fs_tmp->log, "   Sale Fijo 256: size-writed: %d", (size-writed));
			//mensaje.cod_return = 1; //hay mas
		}
		mensaje.size = partSize;
		//log_trace(fs_tmp->log, "    Part - Size:%d - Off:%d",mensaje.size, mensaje.offset);
		mensaje.offset = partOffset;
		//mensaje.lastmod = fi->fh;
		memcpy(mensaje.pathto, buf + writed, partSize);
		cant = send_socket(&mensaje,fs_tmp->sock);
		if(cant<0){
			log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
			return -EFAULT;
		}
		/*
		 * Espero respuesta del Servidor
		 */
		cant = recv_socket(&mensaje,fs_tmp->sock);
		if(cant <= 0){
			log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
			return -EFAULT;
		}
		if (mensaje.cod_return < 0){
			log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
			return mensaje.cod_return;
		}
		writed = writed + mensaje.cod_return;
		//log_trace(fs_tmp->log, "    writed(%d): %d de %d", mensaje.cod_return, writed, size);
		partOffset += mensaje.cod_return;
	}while (writed < size );
	sem_post(&fs_tmp->mux_socket);
	return writed;
}

int osada_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA ftruncate: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_FTRUNCATE;
	mensaje.len = 290;
	mensaje.offset = offset;
	strcpy(mensaje.path, path);

	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	return mensaje.cod_return;
}

int osada_truncate(const char * path, off_t offset)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA truncate: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_FTRUNCATE;
	mensaje.len = 290;
	mensaje.offset = offset;
	strcpy(mensaje.path, path);
	sem_wait(&fs_tmp->mux_socket);
	int32_t cant;
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.type != OP_FTRUNCATE){
		printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_FTRUNCATE);fflush(stdout);
	}
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	return mensaje.cod_return;
}

int osada_unlink (const char *path)
{printf("\nUNLINK: %s ", path);fflush(stdout);
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA unlink: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_UNLINK;
	mensaje.len = 290;
	strcpy(mensaje.path, path);
	sem_wait(&fs_tmp->mux_socket);
	int32_t cant;
	printf("Send ");fflush(stdout);
 	cant = send_socket(&mensaje,fs_tmp->sock);
 	printf("OK - ");fflush(stdout);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
 	printf("Recv ");fflush(stdout);
	cant = recv_socket(&mensaje,fs_tmp->sock);
 	printf("OK - ");fflush(stdout);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
 	if (mensaje.type != OP_UNLINK){
		printf("...MENSAJE ERRONEO!!! %d != %d",mensaje.type, OP_UNLINK);fflush(stdout);
	}
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
 	printf("Fin UNLINK");fflush(stdout);
	return mensaje.cod_return;
}

int osada_utimens(const char* path, const struct timespec ts[2])
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA UTIMENS: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_UTIMENS;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	mensaje.lastmod = ts[1].tv_sec;
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

int osada_release(const char* path, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA RELEASE: %s", path);
	printf("OSADA RELEASE: %s", path);fflush(stdout);
	osada_packet mensaje;
	mensaje.type = OP_RELEASE;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
	sem_wait(&fs_tmp->mux_socket);
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}

	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		sem_post(&fs_tmp->mux_socket);
		log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
		return -EFAULT;
	}
	sem_post(&fs_tmp->mux_socket);
	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
	printf("Fin RELEASE");fflush(stdout);
	return 0;
}

static void *osada_init(struct fuse_conn_info *conn)
{
	conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;

	/*fs_osada_t *fs_tmp = calloc(sizeof(fs_osada_t), 1);
	char arch[15] = "logFUSE.txt";
	fs_tmp->log = log_create( arch, "PokCli" , false , LOG_LEVEL_INFO);
	//sleep(2);
	if ((fs_tmp->log) == NULL){
		sleep(10);
		return EXIT_FAILURE;
	}
	//sleep(2);
	if((fs_tmp->sock = create_socket())<0) {
		log_error(fs_tmp->log, "Error al crear el socket: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else {log_info(fs_tmp->log,"Create Socket OK");}

	if(connect_socket(fs_tmp->sock, (char *)"127.0.0.1", 3001)){
		log_error(fs_tmp->log, "La conexion al Servidor no esta lista: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else {log_info(fs_tmp->log,"Connect Socket OK");}
*/


	osada_packet mensaje;

	mensaje.type = 0;
	mensaje.len = 0;

	if(send_socket(&mensaje, fs_tmp->sock)<0)
		log_error(fs_tmp->log, "Error al enviar Pedido de conexión");
	else
		log_info(fs_tmp->log, "Envio de conexion OK");

	if(recv_socket(&mensaje, fs_tmp->sock)>0)
	{
		log_info(fs_tmp->log, "Recibio Type:%d.",mensaje.type);
		if(mensaje.type == OK)
			log_info(fs_tmp->log, "Conexion al Pokedex Servidor OK.");

	}else log_error(fs_tmp->log, "Fallo la conexion al Pokedex Servidor.");
	sem_init(&fs_tmp->mux_socket,0,1);
	return fs_tmp;
}

void osada_destroy(void * foo) //foo es private_data que devuelve el init
{
	log_info(((fs_osada_t *)foo)->log, "----- OSADA DESTROY -----");

/*	osada_packet mensaje;
	mensaje.type = OP_DESTROY;
	mensaje.len = 0;
	int32_t cant;
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0)
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
*/
	sem_destroy(&((fs_osada_t *)foo)->mux_socket);
	close_socket(((fs_osada_t *)foo)->sock);
    log_destroy(((fs_osada_t *)foo)->log);
    free(foo);
}

static struct fuse_operations osada_oper = {
	.getattr   = osada_getattr,
	.mkdir     = osada_mkdir,
	.unlink    = osada_unlink,
	.rmdir     = osada_rmdir,
	.rename    = osada_rename,
	.truncate  = osada_truncate,
	.utimens   = osada_utimens,
	.open      = osada_open,
	.read      = osada_read,
	.write     = osada_write,
	.statfs    = osada_statfs,
	.release   = osada_release,
	.readdir   = osada_readdir,
	.create    = osada_create,
	.ftruncate = osada_ftruncate,
	.init      = osada_init,
};

int main ( int argc , char * argv []) {
	printf("Pokedex Cliente!\n");

	fs_osada_t *fs_tmp = calloc(sizeof(fs_osada_t), 1);
	char arch[15] = "logFUSE.txt";
	fs_tmp->log = log_create( arch, "PokCli" , false , LOG_LEVEL_INFO);
	if ((fs_tmp->log) == NULL){
		usleep(100);
		return EXIT_FAILURE;
	}
	if((fs_tmp->sock = create_socket())<0) {
		log_error(fs_tmp->log, "Error al crear el socket: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else {log_info(fs_tmp->log,"Create Socket OK");}

	if(connect_socket(fs_tmp->sock, (char *)argv[argc-3], atoi(argv[argc-2]))){
		log_error(fs_tmp->log, "La conexion al Servidor no esta lista: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else {log_info(fs_tmp->log,"Connect Socket OK");}

	argv[argc-3] = argv[argc-1];
	argv[argc-2] = NULL;
	argv[argc-1] = NULL;
	argc--;
	argc--;

	printf("\nInicia FUSE\n");
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, fs_tmp);
	printf("\nFinalizo: %d", ret);
	return ret;
}
