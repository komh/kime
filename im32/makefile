.SUFFIXES : .c .o .obj
CC = gcc
CFLAGS = -O3 -Wall -Wno-parentheses -Wno-format -DDEBUG -g
CFLAGS2 = -Zomf -Zsys -s -O3 -Wall -Wno-parentheses -Wno-format

RM = rm
CP = cp

.c.o:
	$(CC) $*.c $(CFLAGS) -c -o $@
.c.obj:
	$(CC) $*.c $(CFLAGS2) -c -o $@

#targets
debug: im32.a
release: im32.lib
all: im32.a im32.lib

#hanlib
im32.a:  im32.o toggle.o
	ar cvr im32.a $?

im32.lib: im32.obj toggle.obj
	emxomfar cvr im32.lib $?

im32.o: im32.c im32.h

toggle.o: toggle.c toggle.h

im32.obj: im32.c im32.h

toggle.obj: toggle.c toggle.h

touch:
	touch *.c *.h

clean:
	-$(RM) *.o *.obj *.a *.lib core *.bak *~
