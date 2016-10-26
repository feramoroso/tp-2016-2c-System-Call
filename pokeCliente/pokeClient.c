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
	//log_info(fs_tmp->log, "OSADA read: %s-%d-%d", path, (int)offset, (int)size);
	/*
		osada_send(sock, Operacion, Parametros (path, buf, size, offset, fi));
	
		Resultados posibles:
		-	Todo bien
				return CANTIDAD_LEIDO;
		-	log_error(fs_tmp->log, "El archivo no se encuentra abierto.");
				return -EINVAL;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe la carpeta");
				return -ENOENT;
	*/return 0;
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
	log_trace(fs_tmp->log, "    Cant Files: %d", cantFiles);
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
		log_trace(fs_tmp->log, "    %d - File: %s", i, mensaje.fname);
		i++;
		if (i < cantFiles){
			cant = recv_socket(&mensaje,fs_tmp->sock);
			if(cant <= 0){
				log_error(fs_tmp->log, "    ERROR recv: %s", strerror(errno));
				return -EFAULT;
			}

		}

	}
	/*
		osada_send(sock, Operacion, Parametros (path, buf, filler, offset, fi));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe la carpeta");
				return -ENOENT;
		-	log_error(fs_tmp->log, "No hay espacio disponible.");
				return -ENOSPC; //EFBIG - ENOSPC;
		-	log_error(fs_tmp->log, "Ya existe la carpeta.");
				return -EEXIST;

		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

	*/
	return mensaje.cod_return;
}

int osada_mkdir (const char *path, mode_t mode)
{
	//log_info(fs_tmp->log, "OSADA mkdir: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, mode));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "El nombre excede el limite de NRO caracteres impuesto por el enunciado.";
				return -ENAMETOOLONG;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe la carpeta");
				return -ENOENT;
		-	log_error(fs_tmp->log, "No hay espacio disponible.");
				return -ENOSPC; //EFBIG - ENOSPC;
		-	log_error(fs_tmp->log, "Ya existe la carpeta.");
				return -EEXIST;
	*/return 0;
}

int osada_rmdir (const char *path)
{
	//log_info(fs_tmp->log, "OSADA rmdir: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe la carpeta");
				return -ENOENT;
		-	log_error(fs_tmp->log, "La carpeta no esta vacia.");
				return -ENOTEMPTY;
		-	log_error(fs_tmp->log, "Ya existe la carpeta.");
				return -EEXIST;
	*/return 0;
}

int osada_rename (const char *from, const char *to)
{
	//log_info(fs_tmp->log, "OSADA rename: %s - %s", from, to);
	/*
		osada_send(sock, Operacion, Parametros (from, to));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "El nombre excede el limite de NRO caracteres impuesto por el enunciado.";
				return -ENAMETOOLONG;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo");
				return -ENOENT;
		-	log_error(fs_tmp->log, "El archivo no se encuentra abierto.");
				return -EINVAL;
		-	log_error(fs_tmp->log, "No hay espacio disponible.");
				return -ENOSPC; //EFBIG - ENOSPC;
		-	log_error(fs_tmp->log, "Ya existe un archivo con el nombre.");
				return -EEXIST;
	*/return 0;
}

static int osada_open(const char *path, struct fuse_file_info *fi)
{
	//log_info(fs_tmp->log, "OSADA open: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, fi));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "El nombre excede el limite de NRO caracteres impuesto por el enunciado.";
				return -ENAMETOOLONG;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo");
				return -ENOENT;
	*/return 0;
}

int osada_create (const char *path, mode_t mode, struct fuse_file_info *fi)
{
	//log_info(fs_tmp->log, "OSADA create: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, mode, fi));
	
		Resultados posibles:
		-	Todo bien
				osada_open(path, fi);
				return 0;
		-	log_error(fs_tmp->log, "El nombre excede el limite de NRO caracteres impuesto por el enunciado.";
				return -ENAMETOOLONG;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
		-	log_error(fs_tmp->log, "El archivo no se encuentra abierto.");
				return -EINVAL;
		-	log_error(fs_tmp->log, "No hay espacio disponible.");
				return -ENOSPC; //EFBIG - ENOSPC;
		-	log_error(fs_tmp->log, "Ya existe un archivo con el nombre.");
				return -EEXIST;
	*/return 0;
}

int osada_statfs(const char *path, struct statvfs *statvfs)
{
	//log_info(fs_tmp->log,"OSADA statfs: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, statvfs));
	
		Resultados posibles:
		-	Todo bien
				return 0;
	*/return 0;
}

int osada_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
	//log_info(fs_tmp->log, "OSADA write: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, bif, size, offset, fi));

		Resultados posibles:
		-	Todo bien
				return CANTIDAD_COPIADO;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
		-	log_error(fs_tmp->log, "El archivo no se encuentra abierto.");
				return -EINVAL;
		-	log_error(fs_tmp->log, "No hay espacio suficiente.");
				return -EFBIG; //EFBIG - ENOSPC
	*/return 0;
}

int osada_ftruncate (const char *path, off_t offset, struct fuse_file_info *fi)
{
	//log_info(fs_tmp->log, "OSADA ftruncate: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, offset, fi));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
		-	log_error(fs_tmp->log, "El archivo no se encuentra abierto.");
				return -EINVAL;
		-	log_error(fs_tmp->log, "No hay espacio suficiente.");
				return -EFBIG; //EFBIG - ENOSPC
	*/return 0;
}

int osada_truncate(const char * path, off_t offset)
{
	//log_info(fs_tmp->log, "OSADA truncate: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path, offset));
	
		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
	*/return 0;
}

int osada_unlink (const char *path)
{
	//log_info(fs_tmp->log, "OSADA unlink: %s", path);
	/*
		osada_send(sock, Operacion, Parametros (path));

		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
	*/return 0;
}

int osada_utimens(const char* path, const struct timespec ts[2]){

	//log_info(fs_tmp->log, "OSADA UTIMENS: %s", path);
	
	/*
		osada_send(sock, Operacion, Parametros (path, timespec));
	
		Resultados posibles:
		-	Todo bien
				return 0;
		-	log_error(fs_tmp->log, "No existe la ruta indicada");
				return -EFAULT;
		-	log_error(fs_tmp->log, "No existe el archivo a eliminar");
				return -ENOENT;
	*/return 0;
}
static void *osada_init(struct fuse_conn_info *conn)
{
	conn->want |= FUSE_CAP_ATOMIC_O_TRUNC;
	fs_osada_t *fs_tmp = calloc(sizeof(fs_osada_t), 1);

	fs_tmp->log = log_create("logFUSE.txt" , "PokedexCliente" , true , LOG_LEVEL_TRACE);

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

static struct fuse_operations osada_oper = {
	.getattr   = osada_getattr,
	.readdir   = osada_readdir,
	.init      = osada_init,
	//.open      = osada_open,
	//.create    = osada_create,
	//.read      = osada_read,
	//.truncate  = osada_truncate,
	//.write     = osada_write,
	//.unlink    = osada_unlink,
	//.rmdir     = osada_rmdir,
	//.ftruncate = osada_ftruncate,
	//.mkdir     = osada_mkdir,
	//.rename    = osada_rename,
	//.statfs    = osada_statfs,
	//.utimens   = osada_utimens,
};

int main ( int argc , char * argv []) {
	printf("Pokedex Cliente !\n");

	printf("\nInicia FUSE\n");
	int ret=0;
	ret = fuse_main(argc, argv, &osada_oper, NULL);
	return ret;
}
