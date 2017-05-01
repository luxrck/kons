CC	:= gcc -pipe
OBJDUMP	:= objdump

CFLAGS	:= -std=gnu99 -Wall -Wno-unused -Werror -g0 -O3 -march=native
LFLAGS	:= `pkg-config --libs --cflags glib-2.0 fontconfig freetype2 libdrm` -Isrc -lm -lutil

SRC	:= src/charset.c	\
	src/options.c		\
	src/cursor.c		\
	src/cache.c		\
	src/font.c		\
	src/drm.c		\
	src/vt.c		\
	src/main.c

OBJ	:= $(patsubst %.c, %.o, $(SRC))

%.o: %.c
	$(CC) $(LFLAGS) $(CFLAGS) -c -o $@ $<

kons: $(OBJ)
	$(CC) -o $@ $(CFLAGS) $(LFLAGS) $(SRC)
	$(OBJDUMP) -S $@ > $@.asm


all:
	make kons

clean:
	rm -f src/*.o src/*.d src/*.gch kons*
