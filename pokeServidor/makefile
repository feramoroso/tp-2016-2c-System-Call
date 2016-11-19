FLAGS      = 
LIBFLAGS   = -lpthread -lcommons 

PFS_BIN =	./pokeServidor
PFS_OBJS =	./socks_fs.o \
				./pokeServ.o \

PFS_INCLUDES =	./socks_fs.h \
					./pokeServ.h \


all: pokeServ


pokeServ: $(PFS_BIN)

$(PFS_BIN): $(PFS_OBJS) $(PFS_INCLUDES)
	@gcc $(PFS_OBJS) -o $@ $(LIBFLAGS)

%.o: %.c $(PFS_INCLUDES)
	@gcc $(FLAGS) $< -o $@ -c


.PHONY: pokeServ all