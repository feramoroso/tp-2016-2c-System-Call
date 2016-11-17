#gcc -DFUSE_USE_VERSION=27 -D_FILE_OFFSET_BITS=64 
#gcc  -o "pokeCliente" $(OBJS) $(USER_OBJS) $(LIBS) -c pokeClient.c

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
./Debug/pokeClient.c \
./Debug/socks_fs.c 

OBJS += \
./Debug/pokeClient.o \
./Debug/socks_fs.o 

C_DEPS += \
./pokeClient.d \
./socks_fs.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DFUSE_USE_VERSION=27 -D_FILE_OFFSET_BITS=64 -O0 -g3 -Wall -g -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


# All Target
all: pokeCliente

# Tool invocations
pokeCliente: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	gcc  -o "pokeCliente" $(OBJS) -lcommons -lpthread -lfuse
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(OBJS)$(C_DEPS)$(EXECUTABLES) pokeCliente
	-@echo ' '

.PHONY: all clean dependents



