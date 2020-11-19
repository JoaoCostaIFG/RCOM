NAME = download
SRC = download.c
OBJ = ${SRC:.c=.o}

# compiler and linker
CC = gcc
# flags
CFLAGS = -g -Wall -Wextra
# CFLAGS += -DDEBUG

all: options download

options:
	@echo rcom build options:
	@echo "CFLAGS = ${CFLAGS}"
	@echo "CC     = ${CC}"

.c.o:
	${CC} -c ${CFLAGS} $<

download: ${OBJ}
	${CC} ${CFLAGS} -o ${NAME} ${OBJ}

clean:
	rm -f ${NAME} ${OBJ}

.PHONY: all options clean download

