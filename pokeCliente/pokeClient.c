#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fuse.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <commons/log.h>
#include "pokeClient.h"
#include "socks_fs.h"


/*
 * osada_read: Funcion FUSE
 */
static int osada_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA read: %s-%d-%d", path, (int)offset, (int)size);

	osada_packet mensaje;
	mensaje.type = OP_READ;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	mensaje.size = size;
	mensaje.offset = offset;
	//mensaje.lastmod = fi->fh;
	int32_t cant;
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
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
			log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
			return -EFAULT;
		}
		if (mensaje.cod_return < 0){
			log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
			return mensaje.cod_return;
		}
		memcpy(buf + readed, mensaje.path, mensaje.cod_return);
		readed += mensaje.cod_return;
		//log_trace(fs_tmp->log, "mensaje: %s",mensaje.path);
	}
	return readed;
}

static int osada_getattr(const char *path, struct stat *stbuf)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA getattr: %s", path);
	memset(stbuf, 0, sizeof(struct stat));
	//Si path es igual a "/" nos estan pidiendo los atributos del punto de montaje
	if(memcmp(path, "/", 2) == 0) {
	        stbuf->st_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	        stbuf->st_nlink = 2;
	        stbuf->st_uid = context->uid;
	        stbuf->st_gid = context->gid;
	        return 0;
	}
	osada_packet mensaje;
	mensaje.type = OP_GETATTR;
	mensaje.len = 290;//sizeof(osada_packet)-3;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
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

	stbuf->st_mode = S_IRWXU | S_IRWXG | S_IRWXO;
	stbuf->st_nlink = 1;
	stbuf->st_uid = context->uid;
	stbuf->st_gid = context->gid;
	stbuf->st_size = mensaje.size;
	stbuf->st_blksize = OSADA_BLOCK_SIZE;
	stbuf->st_blocks = (mensaje.size / OSADA_BLOCK_SIZE) + 1;

	if (mensaje.file_state == DIRECTORY)
		stbuf->st_mode |= S_IFDIR;
	else
		stbuf->st_mode |= S_IFREG;

	return mensaje.cod_return;
}

static int osada_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;

	log_info(fs_tmp->log, "OSADA readdir: %s", path);
	osada_packet mensaje;
	mensaje.type = OP_READDIR;
	mensaje.len = 290; //sizeof(osada_packet)-3;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
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

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	int32_t i=0,cantFiles=0;
	if (mensaje.offset == 0 )
		return 0;
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
		filler(buf, mensaje.fname, &var_stat, 0);
		//log_trace(fs_tmp->log, "    %d - File: %s", i, mensaje.fname);
		i++;
		if (i < cantFiles){
			cant = recv_socket(&mensaje,fs_tmp->sock);
			if(cant <= 0){
				log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
				return -EFAULT;
			}

		}

	}
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

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

static int osada_open(const char *path, struct fuse_file_info *fi)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA open: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_OPEN;
	mensaje.len = 290;
	strcpy((char *)mensaje.path, path);
	int32_t cant;
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

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	else{
		fi->fh = mensaje.offset;
		log_trace(fs_tmp->log, "   Open OK: %d", mensaje.cod_return);
	}
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
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0){
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
		return -EFAULT;
	}log_info(fs_tmp->log, "    Enviado %d",cant);
	/*
	 * Espero respuesta del Servidor
	 */
	cant = recv_socket(&mensaje,fs_tmp->sock);
	if(cant <= 0){
		log_error(fs_tmp->log, "    ERROR recv(%d): %s", cant, strerror(errno));
		return -EFAULT;
	}

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

	osada_packet mensaje;
	mensaje.type = OP_STATFS;
	mensaje.len = 0;
	int32_t cant;
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
	log_info(fs_tmp->log, "OSADA write: %s", path);

	if (size == 0) return 0;

	int32_t partOffset, partSize, writed, cant;
	osada_packet mensaje;
	partOffset = offset;
	partSize = 0;
	writed = 0;


	mensaje.type = OP_WRITE;
	mensaje.len = 546;
	strcpy(mensaje.path, path);

	while (writed < (int32_t)size ){
		if ((size-writed) <= 256){
			partSize = (size-writed);
			mensaje.cod_return = 0; //ultimo
		}
		else{
			partSize = 256;
			mensaje.cod_return = 1; //hay mas
		}
		mensaje.size = partSize;
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
		}else{
			writed += mensaje.cod_return;
			partOffset = offset + writed;
		}
	}
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

	int32_t cant;
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

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
	return mensaje.cod_return;
}

int osada_unlink (const char *path)
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "OSADA unlink: %s", path);

	osada_packet mensaje;
	mensaje.type = OP_UNLINK;
	mensaje.len = 290;
	strcpy(mensaje.path, path);

	int32_t cant;
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

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));
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

	if (mensaje.cod_return < 0)
		log_error(fs_tmp->log, "    %s", strerror(-mensaje.cod_return));

	return mensaje.cod_return;
}

static void *osada_init(struct fuse_conn_info *conn)
{
	conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	fs_osada_t *fs_tmp = calloc(sizeof(fs_osada_t), 1);

	fs_tmp->log = log_create("logFUSE.txt" , "PokedexCliente" , false , LOG_LEVEL_TRACE);

	if((fs_tmp->sock = create_socket())<0) {
		log_error(fs_tmp->log, "Error al crear el socket: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else log_info(fs_tmp->log,"Create Socket OK");

	if(connect_socket(fs_tmp->sock, (char *)"127.0.0.1", 3001)){
		log_error(fs_tmp->log, "La conexion al Servidor no esta lista: %s", strerror(errno));
		exit(-EADDRNOTAVAIL);
	}else log_info(fs_tmp->log,"Connect Socket OK");
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
	return fs_tmp;
}

void osada_destroy(void * foo) //foo es private_data que devuelve el init
{
	struct fuse_context* context = fuse_get_context();
	fs_osada_t *fs_tmp = (fs_osada_t *) context->private_data;
	log_info(fs_tmp->log, "----- OSADA DESTROY -----");

/*	osada_packet mensaje;
	mensaje.type = OP_DESTROY;
	mensaje.len = 0;
	int32_t cant;
	cant = send_socket(&mensaje,fs_tmp->sock);
	if(cant<0)
		log_error(fs_tmp->log, "    ERROR send: %s", strerror(errno));
*/
	close_socket(((fs_osada_t *)foo)->sock);
    log_destroy(((fs_osada_t *)foo)->log);
    free(foo);
}

static struct fuse_operations osada_oper = {
	.getattr   = osada_getattr,
	.readdir   = osada_readdir,
	.init      = osada_init,
	.mkdir     = osada_mkdir,
	.rmdir     = osada_rmdir,
	.rename    = osada_rename,
	.statfs    = osada_statfs,
	.utimens   = osada_utimens,
	.open      = osada_open,
	.read      = osada_read,
	.create    = osada_create,
	.truncate  = osada_truncate,
	.write     = osada_write,
	.unlink    = osada_unlink,
	.ftruncate = osada_ftruncate,
};

int main ( int argc , char * argv []) {
	printf("Pokedex Cliente!\n");

	printf("\nInicia FUSE\n");
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, NULL);
	return ret;
}
