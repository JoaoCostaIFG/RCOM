NAME_SERVER = noncanonical
SRC_SERVER = noncanonical.c msgutils.c
OBJ_SERVER = ${SRC_SERVER:.c=.o}

NAME_CLIENT = writenoncanonical
SRC_CLIENT = writenoncanonical.c msgutils.c
OBJ_CLIENT = ${SRC_CLIENT:.c=.o}

# compiler and linker
CC = gcc
# flags
CFLAGS = -Wall

all: options rcom

options:
	@echo rcom build options:
	@echo "CFLAGS = ${CFLAGS}"
	@echo "CC     = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

rcom: ${OBJ_SERVER} ${OBJ_CLIENT}
	${CC} ${CFLAGS} -o ${NAME_SERVER} ${OBJ_SERVER}
	${CC} ${CFLAGS} -o ${NAME_CLIENT} ${OBJ_CLIENT}

clean:
	rm -f ${NAME_SERVER} ${OBJ_SERVER} ${NAME_CLIENT} ${OBJ_CLIENT}

.PHONY: all options clean rcom

