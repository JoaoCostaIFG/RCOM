NAME = rcom_tp1
SRC = rcom_tp1.c app_layer.c app_layer_priv.c data_link.c data_link_priv.c vector.c
OBJ = ${SRC:.c=.o}

# compiler and linker
CC = gcc
# flags
CFLAGS = -lm -Wall -Wextra -D HASPROGRESS

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

test: clean all
	@make && ./rcom_tp1 -p 10 -s RECEIVER &
	sleep 0.1
	@./rcom_tp1 -p 11 -s TRANSMITTER -f files/gato.gif
	diff -q files/gato.gif out/files/gato.gif

.PHONY: all options clean rcom

