FLAGS      = -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=27
LIBFLAGS   = -lpthread -lcommons -lfuse

PFS_BIN =	./pokeCliente
PFS_OBJS =	./socks_fs.o \
				./pokeClient.o \

PFS_INCLUDES =	./socks_fs.h \
					./pokeClient.h \


all: pokeCli


pokeCli: $(PFS_BIN)

$(PFS_BIN): $(PFS_OBJS) $(PFS_INCLUDES)
	@gcc $(PFS_OBJS) -o $@ $(LIBFLAGS)

%.o: %.c $(PFS_INCLUDES)
	@gcc $(FLAGS) $< -o $@ -c


.PHONY: pokeCli all