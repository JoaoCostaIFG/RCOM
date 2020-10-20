NAME = rcom_tp1
SRC = rcom_tp1.c app_layer.c data_link.c vector.c
OBJ = ${SRC:.c=.o}

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

rcom: ${OBJ}
	${CC} ${CFLAGS} -o ${NAME} ${OBJ}

clean:
	rm -f ${NAME} ${OBJ}

.PHONY: all options clean rcom

