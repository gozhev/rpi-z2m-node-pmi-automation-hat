DIR_BIN = ./build
DIR_Config = ./lib

OBJ_C = main.c

TARGET = rpi-z2m-node
CC = gcc
CFLAGS := -Wall -std=c11
LIB := -lpaho-mqtt3c -lcjson

OBJ_O = $(patsubst %.c,${DIR_BIN}/%.o,$(notdir ${OBJ_C}))

${TARGET}: ${OBJ_O}
	$(CC) $(CFLAGS) $(OBJ_O) -o $@ $(LIB)

${DIR_BIN}/%.o : %.c | $(DIR_BIN)
	$(CC) $(CFLAGS) -c  $< -o $@ $(LIB)
    
$(DIR_BIN):
	mkdir $@

.PHONY: clean
clean:
	rm $(DIR_BIN)/*.* 
	rm $(TARGET) 
	rmdir $(DIR_BIN)
